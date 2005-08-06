#!/usr/bin/perl -w

use lib '../../tools/pdbgen';

require 'util.pl';

*write_file = \&Gimp::CodeGen::util::write_file;
*FILE_EXT   = \$Gimp::CodeGen::util::FILE_EXT;

$outmk = "Makefile.am$FILE_EXT";
$outignore = ".cvsignore$FILE_EXT";
open MK, "> $outmk";
open IGNORE, "> $outignore";

require 'plugin-defs.pl';

$bins = ""; $opts = "";

foreach (sort keys %plugins) {
    $bins .= "\t";
    if (exists $plugins{$_}->{optional}) {
	$bins .= "\$(\U$_\E)";
	$opts .= "\t$_ \\\n";
    }
    else {
	$bins .= $_;
    }
    $bins .= " \\\n";
}

$extra = "";
foreach (@extra) { $extra .= "\t$_\t\\\n" }
if ($extra) {
    $extra =~ s/\t\\\n$//s;
    $extra = "\t\\\n$extra";
}

foreach ($bins, $opts) { s/ \\\n$//s }

print MK <<EOT;


## ---------------------------------------------------------
## This file is autogenerated by mkgen.pl and plugin-defs.pl
## ---------------------------------------------------------

## Modify those two files instead of this one; for most
## plug-ins you should only need to modify plugin-defs.pl.


libgimpui = \$(top_builddir)/libgimp/libgimpui-\$(GIMP_API_VERSION).la
libgimpwidgets = \$(top_builddir)/libgimpwidgets/libgimpwidgets-\$(GIMP_API_VERSION).la
libgimpconfig = \$(top_builddir)/libgimpconfig/libgimpconfig-\$(GIMP_API_VERSION).la
libgimp = \$(top_builddir)/libgimp/libgimp-\$(GIMP_API_VERSION).la
libgimpcolor = \$(top_builddir)/libgimpcolor/libgimpcolor-\$(GIMP_API_VERSION).la
libgimpbase = \$(top_builddir)/libgimpbase/libgimpbase-\$(GIMP_API_VERSION).la
libgimpmath = \$(top_builddir)/libgimpmath/libgimpmath-\$(GIMP_API_VERSION).la

if OS_WIN32
mwindows = -mwindows
endif

AM_LDFLAGS = \$(mwindows)

libexecdir = \$(gimpplugindir)/plug-ins

EXTRA_DIST = \\
	mkgen.pl	\\
	plugin-defs.pl$extra

INCLUDES = \\
	-I\$(top_srcdir)	\\
	\$(GTK_CFLAGS)	\\
	-I\$(includedir)

libexec_PROGRAMS = \\
$bins

EXTRA_PROGRAMS = \\
$opts

install-\%: \%
	\@\$(NORMAL_INSTALL)
	\$(mkinstalldirs) \$(DESTDIR)\$(libexecdir)
	\@p=\$<; p1=`echo \$\$p|sed 's/\$(EXEEXT)\$\$//'`; \\
	if test -f \$\$p \\
	   || test -f \$\$p1 \\
	; then \\
	  f=`echo "\$\$p1" | sed 's,^.*/,,;\$(transform);s/\$\$/\$(EXEEXT)/'`; \\
	  echo " \$(INSTALL_PROGRAM_ENV) \$(LIBTOOL) --mode=install \$(libexecPROGRAMS_INSTALL) \$\$p \$(DESTDIR)\$(libexecdir)/\$\$f"; \\
	  \$(INSTALL_PROGRAM_ENV) \$(LIBTOOL) --mode=install \$(libexecPROGRAMS_INSTALL) \$\$p \$(DESTDIR)\$(libexecdir)/\$\$f || exit 1; \\
	else :; fi
EOT

print IGNORE <<EOT;
Makefile
Makefile.in
.deps
.libs
EOT

foreach (sort keys %plugins) {
    my $makename = $_;
    $makename =~ s/-/_/g;

    my $libgimp = "";

    if (exists $plugins{$_}->{ui}) {
        $libgimp .= "\$(libgimpui)";
        $libgimp .= "\t\t\\\n\t\$(libgimpconfig)";
        $libgimp .= "\t\\\n\t\$(libgimpwidgets)";
        $libgimp .= "\t\\\n\t\$(libgimp)";
        $libgimp .= "\t\t\\\n\t\$(libgimpcolor)";
        $libgimp .= "\t\t\\\n\t\$(libgimpmath)";
        $libgimp .= "\t\t\\\n\t\$(libgimpbase)";
    } else {
        $libgimp .= "\$(libgimp)";
        $libgimp .= "\t\t\\\n\t\$(libgimpcolor)";
        $libgimp .= "\t\t\\\n\t\$(libgimpbase)";
    }

    my $optlib = "";
    if (exists $plugins{$_}->{optional}) {
	my $name = exists $plugins{$_}->{libopt} ? $plugins{$_}->{libopt} : $_;
	$optlib = "\n\t\$(LIB\U$name\E)\t\t\\";

	if (exists $plugins{$_}->{cflags}) {
	    my $cflags = $plugins{$_}->{cflags};
	    my $optflags = $cflags =~ /FLAGS/ ? $cflags : "\U$_\E_CFLAGS";

	    print MK <<EOT;

${makename}_CFLAGS = \$($optflags)
EOT
        }
    }

    my $deplib = "\$(RT_LIBS)\t\t\\\n\t\$(INTLLIBS)";
    if (exists $plugins{$_}->{libdep}) {
	my @lib = split(/:/, $plugins{$_}->{libdep});
	foreach $lib (@lib) {
	    $deplib = "\$(\U$lib\E_LIBS)\t\t\\\n\t$deplib";
	}
    }

    print MK <<EOT;

${makename}_SOURCES = \\
	$_.c

${makename}_LDADD = \\
	$libgimp		\\$optlib
	$deplib
EOT

    print IGNORE "$_\n";
}

close MK;
close IGNORE;

&write_file($outmk);
&write_file($outignore);
