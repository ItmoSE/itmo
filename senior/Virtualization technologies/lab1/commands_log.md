# Ключевые команды, использованные в лабораторной

## Проверка хоста
```bash
uname -r
df -h ~
pacman -Q virtualbox virtualbox-host-dkms
pacman -Q linux linux-headers
dkms status
VBoxManage --version
VBoxManage list extpacks
lscpu | grep -E 'Virtualization'
```

## Сеть Internal Network
```bash
VBoxManage modifyvm "WS_MolchanovFyodorDenisovich_ubuntu" --nic1 intnet --intnet1 intnet --cableconnected1 on
VBoxManage modifyvm "WS_MolchanovFyodorDenisovich_win" --nic1 intnet --intnet1 intnet --cableconnected1 on
```

Ubuntu:
```bash
sudo nmcli connection modify netplan-enp0s3 ipv4.method manual ipv4.addresses 192.168.99.2/24 ipv4.gateway "" ipv4.dns ""
```

Windows PowerShell:
```powershell
Set-NetIPInterface -InterfaceIndex 7 -Dhcp Disabled
New-NetIPAddress -InterfaceIndex 7 -IPAddress 192.168.99.1 -PrefixLength 24
New-NetFirewallRule -DisplayName "Allow ICMPv4 Echo Request" -Protocol ICMPv4 -IcmpType 8 -Direction Inbound -Action Allow
```

## Host-Only
```bash
VBoxManage hostonlyif create
VBoxManage hostonlyif ipconfig vboxnet0 --ip=192.168.56.1 --netmask=255.255.255.0
printf '%s\n' '* 192.168.56.0/21 192.168.99.0/24' | sudo tee /etc/vbox/networks.conf
VBoxManage hostonlyif create
VBoxManage hostonlyif ipconfig vboxnet1 --ip=192.168.99.1 --netmask=255.255.255.0
VBoxManage dhcpserver add --interface=vboxnet1 --server-ip=192.168.99.2 --netmask=255.255.255.0 --lower-ip=192.168.99.10 --upper-ip=192.168.99.77 --enable
```

## NAT и NAT Network
```bash
VBoxManage modifyvm "WS_MolchanovFyodorDenisovich_ubuntu" --nic1 nat --cableconnected1 on
VBoxManage modifyvm "WS_MolchanovFyodorDenisovich_win" --nic1 nat --cableconnected1 on

VBoxManage natnetwork add --netname NatNetwork --network "10.45.33.0/24" --dhcp on --enable
VBoxManage natnetwork start --netname NatNetwork
VBoxManage dhcpserver modify --network=NatNetwork --lower-ip=10.45.33.4 --upper-ip=10.45.33.254

VBoxManage natnetwork add --netname NatNetwork1 --network "10.22.77.0/24" --dhcp on --enable
VBoxManage natnetwork start --netname NatNetwork1
VBoxManage dhcpserver modify --network=NatNetwork1 --lower-ip=10.22.77.4 --upper-ip=10.22.77.254
```

## Управление VM
```bash
VBoxManage list vms
VBoxManage list runningvms
VBoxManage showvminfo "WS_MolchanovFyodorDenisovich_ubuntu"
VBoxManage startvm "WS_MolchanovFyodorDenisovich_ubuntu" --type gui
VBoxManage startvm "WS_MolchanovFyodorDenisovich_win" --type gui
```
