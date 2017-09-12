# clean up
rm -rf ./blk/block.dat
touch ./blk/block.dat
rm -rf ./log/provenance.log
touch ./log/provenance.log
sync

# start provd
./provd -d > ./provd.log &

# start kernel
modprobe g_mass_storage file=daveti2.img key_file=arpsec03_aik_pub.key force_fail=0
