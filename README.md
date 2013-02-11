nagios-plugins-filesystems
==========================

## check_readonlyfs

This Nagios plugin checks for readonly filesystems.

Usage

	check_readonlyfs [OPTION]... [FILE]...
	check_readonlyfs --help
	check_readonlyfs --version

Options 

	-l, --local               limit listing to local file systems
	-L, --list                display the list of checked file systems
	-T, --type=TYPE           limit listing to file systems of type TYPE
	-X, --exclude-type=TYPE   limit listing to file systems not of type TYPE
	-h, --help                display this help and exit
	-v, --version             output version information and exit

Examples

	check_readonlyfs
	check_readonlyfs -l -T ext3 -T ext4
	check_readonlyfs -l -X vfat

## Source code

The source code can be also found at
https://sites.google.com/site/davidemadrisan/opensource


## Installation

This package uses GNU autotools for configuration and installation.

If you have cloned the git repository then you will need to run `autogen.sh`
to generate the required files.

Run `./configure --help` to see a list of available install options.
The plugin will be installed by default into `LIBEXECDIR`.

It is highly likely that you will want to customise this location to suit your
needs, i.e.:

	./configure --libexecdir=/usr/lib/nagios/plugins

After `./configure` has completed successfully run `make install` and you're
done!


## Supported Platforms

This package is written in plain C, making as few assumptions as possible, and
sticking closely to ANSI C/POSIX.

This is a list of platforms this nagios plugin is known to compile and run on

* Linux openmamba 2.75, with kernel 3.4, gcc 4.7.2, and glibc 2.16.0
* SunOS openindiana 5.11 with gcc 4.3.3
* OpenBSD 5.2 with gcc 4.2.1
* AIX 6.1 with gcc 4.2.0

## Bugs

If you find a bug please create an issue in the project bug tracker at
https://github.com/madrisan/nagios-plugins-filesystems/issues
