echo "--------------------------Make--------------------------"
make
echo "--------------------------Remove------------------------"
sudo rmmod mp1
echo "--------------------------Install--------------------------"
sudo insmod mp1.ko
