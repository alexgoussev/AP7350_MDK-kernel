#!/usr/bin/perl
($#ARGV != 0) && &Usage;
$prj = $ARGV[0];
$prjmk = "mediatek/config/${prj}/ProjectConfig.mk";
$prjmk = "mediatek/config/common/ProjectConfig.mk";

print "\n";
print "# begin mediatek build properties\n";


foreach $prjmk ("device/raindi/${prj}/ProjectConfig.mk", "device/raindi/${prj}/ProjectConfig_ckt.mk") {
  if (!-e $prjmk) {
	unless ($_ eq "device/raindi/${prj}/ProjectConfig_ckt.mk")
  	{
    die "#### Can't find $prjmk\n";
    }
  } else {
    open (FILE_HANDLE, "<$prjmk") or die "cannot open $prjmk\n";
    while (<FILE_HANDLE>) {
    	if(s/#.+//)
		{
		}

      if (/^(\S+)\s*=\s*(.+)\s*$/) {
        $$1 = $2;
      }
    }
    close FILE_HANDLE;
  }
}

if (
      (! -e "ckt/$ENV{CUST_NAME}/system/etc/getsystemtype.sh")
   || ( "$ENV{CKT_VERSION_AUTO_SWITCH}" ne "yes")
   )#如果不存在此文件才产生,否则自动产生,或者CKT_VERSION_AUTO_SWITCH不等于yes 苏 勇 2013年08月05日13:49:05 
{
	print "ro.mediatek.version.release=$ENV{CKT_BUILD_VERNO}\n";
}
print "ro.mediatek.platform=$MTK_PLATFORM\n";
print "ro.mediatek.chip_ver=$MTK_CHIP_VER\n";
print "ro.mediatek.version.branch=$MTK_BRANCH\n";
print "ro.mediatek.version.sdk=$PLATFORM_MTK_SDK_VERSION\n";
print "# end mediatek build properties\n";



#// 增加对modem的hash解析 苏 勇 2013年08月26日11:06:00 
my $modemhash=&GetModeHash();
print "ro.raindi.modem.hash=$modemhash\n";

exit 0;


sub GetModeHash()
{
	my $allhash="";
	
	@custom_modem=split /\s/,${CUSTOM_MODEM};

	foreach my $modem (@custom_modem)
	{
		my @modemfile=<vendor/mediatek/proprietary/custom/${prj}/modem/${modem}/*.img>;
		my $modemhash;
		if (scalar(@modemfile) == 1)
		{
			$modemhash=`grep -awoe 'HASH_[0-9,a-z]\\{7\\}' $modemfile[0]`;
			unless (length($modemhash) == 13)
			{
				print "#modem Hash未找到,或者找到多个modemhash=$modemhash  grep -awoe 'HASH_[0-9,a-z]\\{7\\}' $modemfile[0]\n";
				$modemhash="fffffff";
				$modemfile[0]="vendor/mediatek/proprietary/custom/${prj}/modem/${modem}/unknown";
			}
			$modemhash=~s/HASH_//;
		}
		else
		{
			print "#modem文件未找到,或者找到多个 @modemfile\n";
			$modemhash="fffffff";
			$modemfile[0]="vendor/mediatek/proprietary/custom/${prj}/modem/${modem}/unknown";
		}
		chomp ($modemhash);
		$modemfile[0]=~s{vendor/mediatek/proprietary/custom/${prj}/modem/${modem}/}{};
		$allhash.="/$modemfile[0]($modemhash)";
	}

	$allhash=~s/^\///;
	return $allhash;
}
