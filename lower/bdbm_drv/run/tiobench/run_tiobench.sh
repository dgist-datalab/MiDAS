#sudo mkdir /media/blueDBM/1
#sudo tiobench --numruns 100 --size 100 --threads 4 --random 100000 --dir /media/blueDBM/1 &
#sudo mkdir /media/blueDBM/2
#sudo tiobench --numruns 100 --size 100 --threads 4 --random 100000 --dir /media/blueDBM/2 &
#sudo mkdir /media/blueDBM/3
#sudo tiobench --numruns 100 --size 100 --threads 4 --random 100000 --dir /media/blueDBM/3 &

# for 2GB / 12 threads
#sudo mkdir /media/blueDBM/1
#sudo tiobench --numruns 10 --size 450 --threads 4 --random 100000 --dir /media/blueDBM/1 &
#sudo mkdir /media/blueDBM/2
#sudo tiobench --numruns 10 --size 450 --threads 4 --random 100000 --dir /media/blueDBM/2 &
#sudo mkdir /media/blueDBM/3
#sudo tiobench --numruns 10 --size 450 --threads 4 --random 100000 --dir /media/blueDBM/3 &

# for 2 GB / 24 threads
#sudo mkdir /media/blueDBM/1
#sudo tiobench --numruns 10 --size 450 --threads 8 --random 50000 --dir /media/blueDBM/1 &
#sudo mkdir /media/blueDBM/2
#sudo tiobench --numruns 10 --size 450 --threads 8 --random 50000 --dir /media/blueDBM/2 &
#sudo mkdir /media/blueDBM/3
#sudo tiobench --numruns 10 --size 450 --threads 8 --random 50000 --dir /media/blueDBM/3 &

# for 2 GB / 24 threads
sudo mkdir /media/blueDBM/1
sudo tiobench.pl --numruns 10 --size 450 --threads 16 --random 25000 --target /media/blueDBM/1 &
sudo mkdir /media/blueDBM/2
sudo tiobench.pl --numruns 10 --size 450 --threads 16 --random 25000 --target /media/blueDBM/2 &
sudo mkdir /media/blueDBM/3
sudo tiobench.pl --numruns 10 --size 450 --threads 16 --random 25000 --target /media/blueDBM/3 &

# for 2 GB / 3 threads
#sudo mkdir /media/blueDBM/1
#sudo tiobench --numruns 160 --size 10 --threads 1 --random 250000 --dir /media/blueDBM/1 &
#sudo mkdir /media/blueDBM/2
#sudo tiobench --numruns 160 --size 10 --threads 1 --random 250000 --dir /media/blueDBM/2 &
#sudo mkdir /media/blueDBM/3
#sudo tiobench --numruns 160 --size 10 --threads 1 --random 250000 --dir /media/blueDBM/3 &

for job in `jobs -p`
do
	echo $job
	wait $job
done

