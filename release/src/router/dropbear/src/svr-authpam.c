/*
 * Dropbear SSH
 * 
 * Copyright (c) 2004 Martin Carlsson
 * Portions (c) 2004 Matt Johnston
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE. */

/* Validates a user password using PAM */

#include "includes.h"
#include "session.h"
#include "buffer.h"
#include "dbutil.h"
#include "auth.h"
#include "runopts.h"

#if DROPBEAR_SVR_PAM_AUTH

#if defined(HAVE_SECURITY_PAM_APPL_H)
#include <security/pam_appl.h>
#elif defined (HAVE_PAM_PAM_APPL_H)
#include <pam/pam_appl.h>
#endif

struct UserDataS {
	char* user;
	char* passwd;
};

/* PAM conversation function - for now we only handle one message */
int 
pamConvFunc(int num_msg, 
		const struct pam_message **msg,
		struct pam_response **respp, 
		void *appdata_ptr) {

	int rc = PAM_SUCCESS;
	struct pam_response* resp = NULL;
	struct UserDataS* userDatap = (struct UserDataS*) appdata_ptr;
	unsigned int msg_len = 0;
	unsigned int i = 0;
	char * compare_message = NULL;

	TRACE(("enter pamConvFunc"))

	if (num_msg != 1) {
		/* If you're getting here - Dropbear probably can't support your pam
		 * modules. This whole file is a bit of a hack around lack of
		 * asynchronocity in PAM anyway. */
		dropbear_log(LOG_INFO, "pamConvFunc() called with >1 messages: not supported.");
		return PAM_CONV_ERR;
	}

	/* make a copy we can strip */
	compare_message = m_strdup((*msg)->msg);
	
	/* Make the string lowercase. */
	msg_len = strlen(compare_message);
	for (i = 0; i < msg_len; i++) {
		compare_message[i] = tolower(compare_message[i]);
	}

	/* If the string ends with ": ", remove the space. 
	   ie "login: " vs "login:" */
	if (msg_len > 2 
			&& compare_message[msg_len-2] == ':' 
			&& compare_message[msg_len-1] == ' ') {
		compare_message[msg_len-1] = '\0';
	}

	switch((*msg)->msg_style) {

		case PAM_PROMPT_ECHO_OFF:

			if (!(strcmp(compare_message, "password:") == 0)) {
				/* We don't recognise the prompt as asking for a password,
				   so can't handle it. Add more above as required for
				   different pam modules/implementations. If you need
				   to add an entry here please mail the Dropbear developer */
				dropbear_log(LOG_NOTICE, "PAM unknown prompt '%s' (no echo)",
						compare_message);
				rc = PAM_CONV_ERR;
				break;
			}

			/* You have to read the PAM module-writers' docs (do we look like
			 * module writers? no.) to find out that the module will
			 * free the pam_response and its resp element - ie we _must_ malloc
			 * it here */
			resp = (struct pam_response*) m_malloc(sizeof(struct pam_response));
			memset(resp, 0, sizeof(struct pam_response));

			resp->resp = m_strdup(userDatap->passwd);
			m_burn(userDatap->passwd, strlen(userDatap->passwd));
			(*respp) = resp;
			break;


		case PAM_PROMPT_ECHO_ON:

			if (!(
				(strcmp(compare_message, "login:" ) == 0) 
				|| (strcmp(compare_message, "please enter username:") == 0)
				|| (strcmp(compare_message, "username:") == 0)
				)) {
				/* We don't recognise the prompt as asking for a username,
				   so can't handle it. Add more above as required for
				   different pam modules/implementations. If you need
				   to add an entry here please mail the Dropbear developer */
				dropbear_log(LOG_NOTICE, "PAM unknown prompt '%s' (with echo)",
						compare_message);
				rc = PAM_CONV_ERR;
				break;
			}

			/* You have to read the PAM module-writers' docs (do we look like
			 * module writers? no.) to find out that the module will
			 * free the pam_response and its resp element - ie we _must_ malloc
			 * it here */
			resp = (struct pam_response*) m_malloc(sizeof(struct pam_response));
			memset(resp, 0, sizeof(struct pam_response));

			resp->resp = m_strdup(userDatap->user);
			TRACE(("userDatap->user='%s'", userDatap->user))
			(*respp) = resp;
			break;

		case PAM_ERROR_MSG:
		case PAM_TEXT_INFO:

			if (msg_len > 0) {
				buffer * pam_err = buf_new(msg_len + 4);
				buf_setpos(pam_err, 0);
				buf_putbytes(pam_err, "\r\n", 2);
				buf_putbytes(pam_err, (*msg)->msg, msg_len);
				buf_putbytes(pam_err, "\r\n", 2);
				buf_setpos(pam_err, 0);

				send_msg_userauth_banner(pam_err);
				buf_free(pam_err);
			}
			break;

		default:
			TRACE(("Unknown message type"))
			rc = PAM_CONV_ERR;
			break;      
	}

	m_free(compare_message);
	TRACE(("leave pamConvFunc, rc %d", rc))

	return rc;
}

/* Process a password auth request, sending success or failure messages as
 * appropriate. To the client it looks like it's doing normal password auth (as
 * opposed to keyboard-interactive or something), so the pam module has to be
 * fairly standard (ie just "what's your username, what's your password, OK").
 *
 * Keyboard interactive would be a lot nicer, but since PAM is synchronous, it
 * gets very messy trying to send the interactive challenges, and read the
 * interactive responses, over the network. */
void svr_auth_pam(int valid_user) {

	struct UserDataS userData = {NULL, NULL};
	struct pam_conv pamConv = {
		pamConvFunc,
		&userData /* submitted to pamvConvFunc as appdata_ptr */ 
	};
	const char* printable_user = NULL;

	pam_handle_t* pamHandlep = NULL;

	char * password = NULL;
	unsigned int passwordlen;

	int rc = PAM_SUCCESS;
	unsigned char changepw;

	/* check if client wants to change password */
	changepw = buf_getbool(ses.payload);
	if (changepw) {
		/* not implemented by this server */
		send_msg_userauth_failure(0, 1);
		goto cleanup;
	}

	password = buf_getstring(ses.payload, &passwordlen);

	/* We run the PAM conversation regardless of whether the username is valid
	in case the conversation function has an inherent delay.
	Use ses.authstate.username rather than ses.authstate.pw_name.
	After PAM succeeds we then check the valid_user flag too */

	/* used to pass data to the PAM conversation function - don't bother with
	 * strdup() etc since these are touched only by our own conversation
	 * function (above) which takes care of it */
	userData.user = ses.authstate.username;
	userData.passwd = password;

	if (ses.authstate.pw_name) {
		printable_user = ses.authstate.pw_name;
	} else {
		printable_user = "<invalid username>";
	}

	/* Init pam */
	if ((rc = pam_start("sshd", NULL, &pamConv, &pamHandlep)) != PAM_SUCCESS) {
		dropbear_log(LOG_WARNING, "pam_start() failed, rc=%d, %s", 
				rc, pam_strerror(pamHandlep, rc));
		goto cleanup;
	}

	/* just to set it to something */
	if ((rc = pam_set_item(pamHandlep, PAM_TTY, "ssh")) != PAM_SUCCESS) {
		dropbear_log(LOG_WARNING, "pam_set_item() failed, rc=%d, %s",
				rc, pam_strerror(pamHandlep, rc));
		goto cleanup;
	}

	if ((rc = pam_set_item(pamHandlep, PAM_RHOST, svr_ses.remotehost)) != PAM_SUCCESS) {
		dropbear_log(LOG_WARNING, "pam_set_item() failed, rc=%d, %s",
				rc, pam_strerror(pamHandlep, rc));
		goto cleanup;
	}

#ifdef HAVE_PAM_FAIL_DELAY
	/* We have our own random delay code already, disable PAM's */
	(void) pam_fail_delay(pamHandlep, 0 /* musec_delay */);
#endif

	/* (void) pam_set_item(pamHandlep, PAM_FAIL_DELAY, (void*) pamDelayFunc); */

	if ((rc = pam_authenticate(pamHandlep, 0)) != PAM_SUCCESS) {
		dropbear_log(LOG_WARNING, "pam_authenticate() failed, rc=%d, %s", 
				rc, pam_strerror(pamHandlep, rc));
		dropbear_log(LOG_WARNING,
				"Bad PAM password attempt for '%s' from %s",
				printable_user,
				svr_ses.addrstring);
#ifdef SECURITY_NOTIFY
		SEND_PTCSRV_EVENT(PROTECTION_SERVICE_SSH,
				RPT_FAIL, svr_ses.hoststring,
				"From dropbear , LOGIN FAIL(authpam)");
#endif
		send_msg_userauth_failure(0, 1);
		goto cleanup;
	}

	if ((rc = pam_acct_mgmt(pamHandlep, 0)) != PAM_SUCCESS) {
		dropbear_log(LOG_WARNING, "pam_acct_mgmt() failed, rc=%d, %s", 
				rc, pam_strerror(pamHandlep, rc));
		dropbear_log(LOG_WARNING,
				"Bad PAM password attempt for '%s' from %s",
				printable_user,
				svr_ses.addrstring);
#ifdef SECURITY_NOTIFY
		SEND_PTCSRV_EVENT(PROTECTION_SERVICE_SSH,
				RPT_FAIL, svr_ses.hoststring,
				"From dropbear , LOGIN FAIL(authpam)");
#endif
		send_msg_userauth_failure(0, 1);
		goto cleanup;
	}

	if (!valid_user) {
		/* PAM auth succeeded but the username isn't allowed in for another reason
		(checkusername() failed) */
#ifdef SECURITY_NOTIFY
		SEND_PTCSRV_EVENT(PROTECTION_SERVICE_SSH,
				RPT_FAIL, svr_ses.hoststring,
				"From dropbear , ACCOUNT FAIL");
#endif
		send_msg_userauth_failure(0, 1);
		goto cleanup;
	}

	if (svr_opts.multiauthmethod && (ses.authstate.authtypes & ~AUTH_TYPE_PASSWORD)) {
			/* successful PAM password authentication, but extra auth required */
			dropbear_log(LOG_NOTICE,
					"PAM password auth succeeded for '%s' from %s, extra auth required",
					ses.authstate.pw_name,
					svr_ses.addrstring);
			ses.authstate.authtypes &= ~AUTH_TYPE_PASSWORD; /* PAM password auth ok, delete the method flag */
			send_msg_userauth_failure(1, 0);  /* Send partial success */
		} else {
			/* successful authentication */
			dropbear_log(LOG_NOTICE, "PAM password auth succeeded for '%s' from %s",
				ses.authstate.pw_name,
				svr_ses.addrstring);
#ifdef SECURITY_NOTIFY
			SEND_PTCSRV_EVENT(PROTECTION_SERVICE_SSH,
				RPT_SUCCESS, svr_ses.hoststring,
				"From dropbear , LOGIN SUCCESS(authpam)");
#endif
			send_msg_userauth_success();
		}
	
cleanup:
	if (password != NULL) {
		m_burn(password, passwordlen);
		m_free(password);
	}
	if (pamHandlep != NULL) {
		TRACE(("pam_end"))
		(void) pam_end(pamHandlep, 0 /* pam_status */);
	}
}

#endif /* DROPBEAR_SVR_PAM_AUTH */
