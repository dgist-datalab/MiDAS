sudo mkdir -p /usr/share/bdbm_drv
sudo touch /usr/share/bdbm_drv/ftl.dat
sudo touch /usr/share/bdbm_drv/dm.dat

# compile f2fs
cd ../../f2fs_org/tools/
make
sudo make install
cd -

#sudo insmod risa_dev_ramdrive_intr.ko
#sudo insmod risa_dev_bluedbm.ko
sudo insmod risa_dev_ramdrive.ko
sudo insmod bdbm_drv_dftl.ko
sudo insmod f2fs.ko
sudo ./bdbm_format /dev/blueDBM
sudo mkfs.f2fs -a 0 -s 2 /dev/blueDBM
sudo mount \-t f2fs \-o discard /dev/blueDBM /media/blueDBM
