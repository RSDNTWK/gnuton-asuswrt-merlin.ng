

                  RELEASE NOTES FOR MICROSOFT FAT BY TUXERA


--- Introduction ---

This file contains information regarding the latest Tuxera FAT delivery. Tuxera 
recommends to read the entire file.

Table of content

 * Installation
 * Usage
 * Configurable Mount Options
 * Contact & Support Information


--- Installation ---

Load the Tuxera FAT kernel module, called tfat.ko, into the kernel:

	insmod tfat.ko

then the Tuxera FAT kernel driver is ready to be used.


--- Usage ---

Mount FAT volumes the following way:

	mount -t tfat device mountpoint

Example:

	mount -t tfat /dev/block/mmcblk1p1 /mnt/sdcard

For details on options available during (re)mount, kindly refer to section 
'Configurable Mount Options'.


--- Configurable Mount Options ---

This section provides details on the available mount options at (re)mount to 
tweak the behaviour of the FAT driver. Default values for mount options depend 
on build options and are normally optimized by Tuxera for optimum performance 
and fail safety. If you want to ensure a particular mount option is in use, you 
need to specify it on the command line or ask Tuxera to make it the default for 
your build target(s). In addition to the generic mount options described by the 
manual page for the mount(8) command (e.g. ro, rw, sync, noatime, etc, please 
see 'man 8 mount' and 'man 5 fstab' for details), the Tuxera FAT driver 
supports the following mount options:

    uid=
    The UID the files will be mounted as.
    Usage:
        -o uid=0

    gid=
    The GID the files will be mounted as.
    Usage:
        -o gid=0

    fmask=
    Sets the file mode creation mask (Only the file permission bits of the mask 
    are used).
    Usage:
        -o fmask=0022

    dmask=
    Sets the directory mode creation mask (Only the permission bits of the mask 
    are used).
    Usage:
        -o dmask=0022

    umask=
    A super set to options 'fmask' & 'dmask'. This option sets a generic mode 
    creation mask for both files and directories (Only the permission bits of 
    the mask are used).
    Usage:
         -o umask=0022

    ioblocksize=SIZE
    The preferred blocksize for efficient filesystem I/O. This blocksize is 
    reported in the st_blksize field of struct stat for the stat*() system call.
    Default = 131072
    Usage:
        -o ioblocksize=131072

    iostreaming=[0|1]
    The iostreaming mount mode gives the highest possible read and write 
    throughput performance and at the same time it has the lowest CPU usage 
    during large file transfers. The minimum block size when iostreaming starts 
    is configurable for both read and write via the min_iostreaming_read_iosize 
    and min_iostreaming_write_iosize mount options. Optimal values are chipset 
    and deployment specific, typically between 64kB and 1 MB.
    Default = 0 (disabled)
    Usage (for enabling):
        -o iostreaming (similar to) -o iostreaming=1
    Usage (for disabling):
        -o iostreaming=0

    min_iostreaming_read_iosize=SIZE
    Sets the minimum block size when iostreaming starts for read.
    Default = 131072
    Usage:
        -o min_iostreaming_read_iosize=131072

    min_iostreaming_write_iosize=SIZE
    Sets the minimum block size when iostreaming starts for write.
    Default = 65536
    Usage:
        -o min_iostreaming_write_iosize=65536

    readahead=SIZE_WITH_SUFFIX
    The maximum read-ahead size (speculatively reading the specified size of 
    file data into memory under the assumption that an application requires it).
    Default = 262144
    Usage:
        -o readahead=4M

    min_prealloc_size=SIZE_WITH_SUFFIX
    Assigns a lower bound on the amount of clusters to preallocate (by 
    default), so as to optimize the allocations and boost I/O performance.
    Default = 0
    Usage:
        -o min_prealloc_size=64k

    max_prealloc_size=SIZE_WITH_SUFFIX
    Assigns a limit on the maximum amount of clusters that can be preallocated 
    in order to optimize allocations and boost I/O performance.
    Default = 134217728
    Usage:
        -o max_prealloc_size=128M

    falloc_skip_zeroing=[0|1]
    Do not zero the blocks that are allocated using the *fallocate() and set 
    the initialized size of the file to its data size provided 
    FALLOC_FL_KEEP_SIZE is not set. This will reveal any garbage data that may 
    be present in the non-zeroed region thus causing a potential security 
    breach.
    Default = 0 (disabled)
    Usage (for enabling):
        -o falloc_skip_zeroing (similar to) -o falloc_skip_zeroing=1
    Usage (for disabling):
        -o falloc_skip_zeroing=0

    fail_safe=[0|1]
    If set, it ensures the integrity of the file system and data under various 
    scenarios like unsafe storage unplugging, system crash and power loss. This 
    option has a performance impact for metadata intensive operations but not 
    for file transfers.
    Default = 1 (enabled)
    Usage (for enabling):
        -o fail_safe (similar to) -o fail_safe=1
    Usage (for disabling):
        -o fail_safe=0

    nobarrier=[0|1]
    Write barriers enforce proper on-disk ordering of file system metadata 
    updates, making volatile storage write caches safe to use, at some 
    performance penalty. If your storages are battery-backed, disabling 
    barriers may safely improve performance.
    Default = 0 (barriers are used)
    Usage (barriers are used):
        -o nobarrier=0
    Usage (barriers are not used):
        -o nobarrier (similar to) -o nobarrier=1

    discard=[0|1]
    Controls whether the filesystem driver should issue a discard request to 
    the underlying device to discard or trim currently unused blocks in the 
    filesystem. This will optimize long term performance and lifetime of SSDs 
    and other flash based storage devices. An alternative to using this option 
    is to run fstrim on a regular basis; please see 'man 8 fstrim'.
    Default = 0 (disabled)
    Usage (for enabling):
        -o discard (similar to) -o discard=1
    Usage (for disabling):
        -o discard=0

    utf8 (default)
    Use full Unicode support thus interpreting the FAT long name characters to 
    be stored in UTF-16 rather than UCS2 and assuming that the Linux side is 
    using UTF-8.
    Usage (for enabling):
        -o utf8

    noutf8
    Disable use of full unicode support thus interpreting the FAT long name 
    characters to be stored as UCS2 and assuming that the Linux side is using 
    whatever the default or specified NLS codepage is (see nls= mount option).
    Usage:
        -o noutf8

    nls=name
    Character set containing the definition of byte encoding used to represent 
    each Unicode character of file names. It is used for Unicode string 
    handling. To be sure you are not missing any files, it is advised to use 
    nls=utf8 which is capable of representing all Unicode characters or even 
    better to use the utf8 option (see above).
    Default = utf8
    Usage:
        -o nls=utf8

    hidden=[show|ignore]
    If set to 'ignore', the directory entries with the 'hidden' attribute set 
    will not be listed if the directory is listed using 'ls', etc. It should 
    however be noted that irrespective of hidden=ignore, all files are still 
    accessible by name, i.e. you can always do ls -l <filename>" for example to 
    specifically show/access the file with the 'hidden' set.
    Default = show
    Usage:
        -o hidden=show
        -o hidden=ignore

    errors=[continue|remount-ro|recover]
    Action to be performed by the file-system when a critical error is 
    encountered.
    continue
        DEFAULT, try to clean-up as much as possible, e.g. marking a corrupt 
        inode as bad so it is no longer accessed, and then continue.
    remount-ro
        Remount the volume read-only if any disk write i/o errors occur or the 
        device becomes inaccessible.
    recover
        Recover as much as possible and continue operation.
    Usage:
        -o errors=continue
        -o errors=remount-ro
        -o errors=recover

    allow_utime
    Specifies the permission required for setting the access/modified times of 
    a directory entry. By default, the permissions are derived from the 
    directory mode creation mask.
    Usage:
        -o allow_utime=0000

    force_rw/noforce_rw
    Force mounting/remounting a volume as read-write if the volume would 
    normally be mounted as read-only. Under normal circumstances when a volume 
    has errors, remounting a volume is not allowed. Enabling this option could 
    result in writing corrupt state to the disk.
    Default = noforce_rw
    Usage (for enabling):
        -o force_rw
    Usage (for disabling):
        -o noforce_rw

    fastmount
    Enables the volume to be mounted immediately.
    Default = disabled
    Usage (for enabling):
        -o fastmount

    dirty_on_write=[0|1]
    Causes the volume to be marked as 'DIRTY' on first write and marked as 
    'CLEAN' on unmount.
    Default = 0 (disabled)
    Usage (for enabling):
        -o dirty_on_write (similar to) -o dirty_on_write=1
    Usage (for disabling):
        -o dirty_on_write=0

    abort_mount_if_dirty=[0|1]
    Abort mount operation if the volume to be mounted is marked dirty and 
    return EUCLEAN error code. A volume that is marked dirty must be repaired 
    with fsck before attempting to mount again.
    Default = 0 (disabled)
    Usage (for enabling):
        -o abort_mount_if_dirty (similar to) -o abort_mount_if_dirty=1
    Usage (for disabling):
        -o abort_mount_if_dirty=0

    abort_on_journal_error=[0|1]
    Abort mount operation if the rename journal cannot be opened/replayed. 
    Also, abort rename operation if journal transaction cannot be started.
    Default = 0 (disabled)
    Usage (for enabling):
        -o abort_on_journal_error (similar to) -o abort_on_journal_error=1
    Usage (for disabling):
        -o abort_on_journal_error=0

    codepage=opt
    Defines the OEM character set used for converting short 8.3 file names.
    Default = 437
    Usage:
        -o codepage=852

    inode32|inode64
    On 32-bit systems, inode numbers are always 32-bit but on 64-bit systems 
    inode numbers can normally be 64-bit which can cause problems with 32-bit 
    user space applications. The option inode32 will force the driver to only 
    use 32-bit inode numbers which should make broken 32-bit user space 
    applications work. The option inode64 will allow the driver to return 
    64-bit inode numbers on 64-bit systems.
    Default = inode32
    Usage (for forcing 32-bit inode numbers):
        -o inode32
    Usage (for allowing 64-bit inode numbers):
        -o inode64

    sloppy=[0|1]
    If set, the driver ignores any unrecognized mount option(s) and continues 
    to go about mounting the device on the specified mount point with other 
    given valid options. The default behavior will result in mount failure.
    Usage (for enabling):
        -o sloppy (similar to) -o sloppy=1
    Usage (for disabling):
        -o sloppy=0

    dalloc=[0|1]
    Controls whether the filesystem driver should make use of delayed 
    allocation. This will improve overall I/O performance.
    Default = 1 (enabled)
    Usage (for enabling):
        -o dalloc (similar to) -o dalloc=1
    Usage (for disabling):
        -o dalloc=0

    fsck=[skip|force]
    Flags modifying online fsck behaviour. Normally online fsck is run if an 
    internal flag indicates that it should be run and not otherwise.
    skip
        Do not run online fsck even if it is needed.
    force
        Run online fsck even if it is not needed.
    Usage (to force fsck to run):
        -o fsck=force
    Usage (to force fsck to be skipped):
        -o fsck=skip

    fsck_delay=opt
    Defines a delay in number of seconds by which to delay the start of online 
    fsck. Without this option it will run immediately on completion of the 
    mount or read-write remount. It can be useful to delay online fsck until 
    the system has finished booting for example or until scanning newly 
    attached storage media has been scanned so putting in a delay so fsck is 
    not run in parallel with immediate i/o intensive tasks is advisable to 
    ensure it cannot impact on these other i/o tasks.
    Usage (example to cause online fsck to be delayed by 5 minutes):
        -o fsck_delay=300

    sda=[0|1]
    Enable SDA compliance mode which modifies driver behaviour to match SD 
    association standard for FAT32 rather than the Microsoft standard. In 
    particular the definition of the flags in the second FAT entry are 
    different and the SDA standard demands that the FSInfo sector has a backup 
    whilst the Microsoft specification says such a backup does not exist.

    tz=UTC
    This option causes the driver to assume that the system time zone is UTC 
    which means that no time zone offset is applied when converting between 
    timestamps on disk and in memory. This is particularly useful when mounting 
    devices (like digital cameras) that are set to UTC in order to avoid 
    problems with the system clock being in local time.

    time_offset=minutes
    Set time zone offset used when converting timestamps between local time 
    which is used on disk and UTC which is used by Linux. This is useful when 
    the time zone set in the kernel via settimeofday(2) is not the time zone 
    used by the FAT volume. Note that this option still does not provide 
    correct time stamps in all cases due to Daylight Savings Time (DST) - time 
    stamps in a different DST setting will be off by one hour.

    update_size_on_end_of_write=[0|1]
    In case the file size has increased because of a write enabling this option 
    forces update of the file size after the data has been successfully written 
    to the disk. By default the size and other metadata is updated lazily later 
    on. When enabled one can be sure the size always reflects the size of the 
    written data. This option is commonly preferred in video recording 
    applications to avoid situations, where recording is interrupted by e.g., 
    sudden power loss, and the apparent file size would show less than the data 
    that was written. Enabling this option is also needed, if the apparent file 
    size is used to measure how much and when the data is written. On the 
    downside enabling this option increases the write amplification and can 
    also decrease write performance.
    Default = 0 (disabled)
    Usage (for enabling):
        -o update_size_on_end_of_write (similar to)
        -o update_size_on_end_of_write=1
    Usage (for disabling):
        -o update_size_on_end_of_write=0


--- Contact & Support Information ---

Please send feedback and support queries to support@tuxera.com.

Besides describing your issue please provide the following information to help 
Tuxera's engineers address the issue:

  - detailed console error messages
  - kernel log by using the 'dmesg' command, or 'cat /proc/kmsg'
  - steps to reproduce the issue

Thank you.

Copyright (c) 2008-2024 Tuxera Inc. All Rights Reserved.

