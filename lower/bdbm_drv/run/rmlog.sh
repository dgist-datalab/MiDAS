sudo service rsyslog stop
sudo rm /var/log/messages*
sudo rm /var/log/syslog*
sudo rm /var/log/kern*
sudo rm /var/log/auth*
sudo rm /var/log/syslog
sudo dmesg -C
sudo service rsyslog start
