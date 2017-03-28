sudo mknod node c $1 0
sudo chmod 777 node
nice ./work 1024 R 50000 & nice ./work 1024 R 10000 &
