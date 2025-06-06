Asuswrt-Merlin
==============

About
-----
Asuswrt is the name of the firmware Asus has developed for
their various router models.  Originally forked from Tomato, 
it has since grown into a very different product, removing 
some more technical features that were part of Tomato, but 
also adding a lot of new original features.

Asuswrt-merlin is a customized version of Asus's firmware. The goal is 
to provide bugfixes and minor enhancements to Asus's firmware, with also 
a few occasional feature additions.  This is done while retaining 
the look and feel of the original firmware, and also ensuring that 
the two codebases remain close enough so it will remain possible 
to keep up with any new features brought by Asus in the original firmware.

This project's goal is NOT to develop yet another firmware filled with 
many features that are rarely used by home users - that is already covered 
by other excellent projects such as Tomato or DD-WRT.

This more conservative approach will also help ensuring the highest 
level of stability possible.  Priority is given to stability over 
performance, and performance over features.



Supported Devices
-----------------

Fully supported devices:

3004.388.x:
 * RT-AX88U
 * RT-AX58U & RT-AX3000 (V1 only)
 * RT-AX86U & RT-AX86S
 * GT-AX11000
 * RT-AX68U
 * GT-AXE11000

3006.102.x:
 * RT-BE96U 
 * GT-BE98_PRO
 * RT-BE88U and RT-BE7200
 * RT-BE86U
 * GT-AX6000
 * ZenWifi Pro XT12
 * GT-AX11000 Pro
 * GT-AXE16000
 * RT-AX86U Pro
 * RT-AX88U Pro
 * RT-BE92U

No longer supported:
 * RT-N16
 * RT-N66U
 * RT-AC66U
 * RT-AC56U
 * RT-AC87U
 * RT-AC3200
 * RT-AC66U_B1 (use the RT-AC68U firmware)
 * RT-AC68U, RT-AC68P, RT-AC68UF (including HW revision C1 and E1)
 * RT-AC68U V3 (use the RT-AC68U firmware)
 * RT-AC68U V4 (use the RT-AC68U firmware)
 * RT-AC1900 & RT-AC1900P (use the RT-AC68U firmware)
 * RT-AC88U
 * RT-AC3100
 * RT-AC5300
 * RT-AC86U
 * RT-AC2900 (use the RT-AC86U firmware)
 * GT-AC2900
 * RT-AX56U



Features
--------
Here is a list of features that Asuswrt-merlin adds over the original 
firmware.



System:
   - Various bugfixes
   - Performance optimizations to some CPU-bound components like OpenVPN
   - Some components were updated to their latest versions, for improved stability and security
   - User scripts that run on specific events such as firewall restart
   - Cron jobs for scheduled tasks
   - Customizable config files for router services
   - Third party software through Entware, with an easy setup script
   - SNMP support (some models)
   - Nano text editor (for more user-friendly script editing)
   - NTP daemon, which can synchronize your client devices
   - Third-party addons, with a management interface (AMTM)
 

Disk sharing:
   - Optionally use shorter share names (folder name only)
   - NFS exporting of USB drives (some models)


Networking:
   - Act as a SMB Master Browser
   - Act as a WINS server
   - Allows tweaking TCP/UDP connection tracking timeouts
   - CIFS client support (for mounting remote SMB share on the router)
   - Advanced OpenVPN client and server.
   - Netfilter ipset module, for efficient blacklist implementation
   - DNS Director - enforcing the use of a specific DNS server, can be applied globally or per client
   - Wireless site survey page
   - Custom DDNS (through a user script)
   - TOR support, individual client access control
   - VPN Director - Policy-based routing for OpenVPN and WireGUard clients (based on source or destination IPs)
   - Redirect NTP client queries to the router's own NTP daemon
   - Cake SQM QoS


Web interface:
   - Performance improvements
   - Optionally save traffic stats to disk (USB or JFFS partition)
   - Enhanced traffic monitoring: adding graphical charts, and traffic monitoring per client IP
   - System info summary page
   - Wifi icon reports the state of both radios
   - Advanced wireless client list display, including automated refresh
   - Redesigned layout of the various System Log sections
   - Editable entries (on some pages)


Note that a number of features which first appeared in Asuswrt-Merlin
have since been integrated/implemented in the official firmware, such
as OpenVPN support.


Installation
------------
Simply flash it like any regular update.  You should not need to 
reset to factory defaults (see note below for exceptions).
You can revert back to an original Asus firmware at any time just
by flashing a firmware downloaded from Asus's website.

If upgrading from Asus firmware 3.0.0.6.102_37000 or newer,
the webui may refuse flashing Asuswrt-Merlin.  In that case,
connect to your router over SSH, and issue the following command:

    nvram set DOWNGRADE_CHECK_PASS=1

After that, you can upload Asuswrt-Merlin through the webui like
any regular firmware upgrade.  This is only required when flashing
Asuswrt-Merlin for the first time.

Note that the archive for some ROG models may contain two different
firmware images.  The one with _rog in the filename uses the original
ROG themed user interface, while the other one uses the traditionnal
blue/grey visuals used by non-ROG models.

If the firmware upgrade fails, try rebooting your router to free 
up sufficient memory, without any USB disk plugged in,
then try flashing it again.

NOTE: resetting to factory default after flashing is 
strongly recommended for the following cases:

- Updating from a firmware version that is more than a few releases older
- Switching from another third party firmware
- Switching from Asus' original firmware, and you were using VPN features

If you run into any issue after an upgrade and you haven't done so,
try doing a factory default reset as well.

Always read the changelog, as mandatory resets will be mentionned 
there when they are necessary.

In all of these cases, do NOT load a saved copy of your settings!
This would be the same thing as NOT resetting at all, as you will 
simply re-enter any invalid setting you wanted to get rid of.  Make 
sure to create a new backup of your settings after reconfiguring.



Documentation
-------------
For documentation on how to use the features that are specific to 
Asuswrt-Merlin, as well as additional guides, please consult the
wiki:

https://github.com/RMerl/asuswrt-merlin.ng/wiki

There are also support forums hosted at SNBForums.

Firmware support: https://www.snbforums.com/forums/asuswrt-merlin.42/
AddOns support: https://www.snbforums.com/forums/asuswrt-merlin-addons.60/


Source code
-----------
The source code can be found on Github:

https://github.com/RMerl/asuswrt-merlin.ng


   
Contact information
-------------------
SmallNetBuilder forums (preferred method: https://www.snbforums.com/forums/asuswrt-merlin.42/ as RMerlin)
Website: https://www.asuswrt-merlin.net/
Github: https://github.com/RMerl
Email: merlin@asuswrt-merlin.net
Mastodon: https://fosstodon.org/@RMerlin
Bluesky: @asuswrt-merlin.net
IRC: RMerlin in channel #asuswrt on Libera

Download: https://www.asuswrt-merlin.net/download

Development news will be posted on Mastodon and Bluesky, 
and the support forums.
You can also keep a closer eye on development as it happens,
through the Github code repository.

For support questions, please use the SmallNetBuilder forums.  There's a 
dedicated Asuswrt-Merlin sub-forum there, under the Asus Wireless section.
The community there is the primary source of technical support.

I want to give my special thanks to Asus for showing an interest in 
this project, and also providing me with support and development 
devices when needed.  I also want to thank everyone that has 
donated through Paypal.  Much appreciated!

Finally, my special thanks to r00t4rd3d for designing the 
Asuswrt-Merlin logo.



Disclaimer
----------
This is the part where you usually put a lot of legalese stuff that nobody 
reads. I'm not a lawyer, so I'll just make it simple, using my own words 
rather than some pre-crafted text that will bore you to death and that 
nobody but a highly paid lawyer would even understand anyway:

I take no responsibility for issues caused by this project. I do my best to 
ensure that everything works fine. If something goes wrong, my apologies.

The Asuswrt-merlin firmware is released under a GPL licence.  In short, you 
are free to use, redistribute and modify it, as long as all the associated 
licences are respected, and that any changes you make to the GPL code is 
made publicly available.

Copyrights belong to the appropriate individuals/entities, under the appropriate 
licences. GPL code is covered by GPL, proprietary code is Copyright their 
respective owners, yadda yadda.

I try my best to honor the licences (as far as I can understand them, as a 
normal human being). Anything GPL or otherwise open-sourced that I modify 
will see my changes published to Github at some point. A release might get 
delayed if I'm working using pre-release code. If it's GPL, it will eventually 
be published - no need to send a volley of legal threats at me.

In any other cases not covered, Common Sense prevails, and I shall also make use 
of Good Will.

Concerning privacy:

The only call back made by this firmware to me is when it checks for the
availability of a new version.  The automated check can be disabled if desired.
More info on the Wiki:

https://github.com/RMerl/asuswrt-merlin.ng/wiki/Privacy-disclosure


--- 
Eric Sauvageau
