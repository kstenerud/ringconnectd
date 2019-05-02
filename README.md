Ringconnectd
============

A modem knocking daemon.

First Version Published: June, 1997

I wrote this because I needed to be able to access my Linux system at home from the school labs, but ISPs would forcibly disconnect modems that remained idle online (line camping). This daemon allowed me to use a payphone (at no cost) to signal my home machine to connect to my ISP and email me its current IP address.

I'm archiving it here because the LSM archives have all disappeared over time.


Original Readme
---------------

Ringconnectd 2.3  (01-Mar-98)

Author:

    Karl Stenerud (kstenerud@mdsi.bc.ca)

Description:

    Ringconnectd monitors the modem and waits for it to ring.  Once it rings,
    ringconncetd connects online and optionally mails you your current IP
    address.  Very useful if your ISP doesn't like line camping.

How it works:

    Just make a configuration file for it (by default it looks for
    /etc/ringconnectd.conf), and then run it from your rc.local if you like.
    It will fork into the background and sleep until it detects the proper
    ring code on the modem line.

    To signal ringconnect, simply pickup a phone and dial the number your modem
    is attached to.  Now let it ring according to the ring code you specified
    with the option "rings", and hang up.  Wait about a minute, and then check
    your mail.  Presto! ringconnect will mail you your IP address (provided you
    specified a smtp server and mailing address) and you can now telnet/FTP/HTTP
    to your machine at home!

Supported platforms:

    Linux.  Between the custom serial routines and its use of the /proc
    filesystem, I doublt it would work on anything else.

Where to get it:

    I've been having no end of trouble with sunsite.  But if it ever gets posted,
    it will be at:
    ftp://sunsite.unc.edu/pub/Linux/system/network/daemons/ringconnectd-2.3.tar.gz

    I maintain my own copy at:
    http://milliways.scas.bcit.bc.ca/~karl/files/ringconnectd-2.3.tar.gz

Compiling:

    make
    cp ringconnectd /usr/sbin
    cp ringconnectd.8 /usr/man/man8

Ringtime (included software):

    The included program "ringtime" is used to calculate the option "ring_time"
    for ringconnectd.  To use it, run the program and then call the number your
    modem is attached to.  Let it ring about 4-5 times and take the most
    consistent value.  Now add 1 to that and use it as your "ring_time" value.

Copyrights and warranties:

    This software is released as freeware under the GNU Public Liscence.
    If you do any significant improvements on it, mail me a copy!
   
    As for warranty, there is none.  Use at your own risk!
