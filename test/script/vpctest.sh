#!/bin/bash

DB_USER="root"
DB_PASSWORD="security421"
MYSQLEXC="mysql -u$DB_USER -p$DB_PASSWORD --database=livecloud -sNe "

#while [ 1 ]; do
dest_ip=127.0.0.1
poolid=10000
vm_poolid=10001
up_subnet=172.16.100
up_ip=90
gw_launch_server=172.16.1.112
vm_launch_server=172.16.1.113
bandw=2
isp=1
vmname0=0
vmname1=1
vm0ip=2
vm1ip=3
vdcnum=4
vmnum=9
ipnum=20

#if [ 0 -eq 1 ]; then
#add ip
i=0
while [ $i -le $ipnum ]; do
    curl -s -X POST -k https://${dest_ip}/pool/addip?poolid=${poolid}\&ip="${up_subnet}.${up_ip}"\&vlan=0\&gateway="${up_subnet}.1"\&mask=24\&isp=$isp 
    ((i++))
    ((up_ip++))
done

up_ip=90
i=0
while [ $i -le $vdcnum ]; do
    #add user
    curl -s -X POST -k https://${dest_ip}/fdbuser/adduser -d "userdata={\"company\":\"yunshan$i\",\"user_type\":\"2\",\"username\":\"zz$i\",\"password\":\"123456\",\"industry\":\"\",\"contracts\":\"zz\",\"phone_num\":\"123456\",\"tenancy\":\"2014-9-2 11:40\",\"email\":\"1@163.com\",\"user_resource\":{\"cpu\":\"\",\"memory\":\"\",\"disk\":\"\",\"isp1\":{\"bandwidth\":\"\",\"ip_num\":\"\"},\"isp2\":{\"bandwidth\":\"\",\"ip_num\":\"\"},\"isp3\":{\"bandwidth\":\"\",\"ip_num\":\"\"},\"isp4\":{\"bandwidth\":\"\",\"ip_num\":\"\"}}}"

    userid=`$MYSQLEXC "select id from fdb_user_v2_0 where company=\"yunshan$i\""`

    #add vpc
    curl -s -X POST -k https://${dest_ip}/fdbvpc/addvpc -d "vpcdata={\"name\":\"vpc$i\",\"description\":\"\",\"userid\":\"$userid\"}"

    ips=`$MYSQLEXC "select id from ip_resource_v2_0 where ip=\"${up_subnet}.${up_ip}\""`
    ((up_ip++))

    vpcid=`$MYSQLEXC "select id from vcloud_v2_0 where name=\"vpc$i\""`

    #add vdc vgw 2vm
    echo add vpc$i vdc$i vm$vmname0 vm$vmname1
    date
    curl -s -X POST -k https://${dest_ip}/fdbvdc/addvdc -d "vdcdata={\"isp1\":{\"ips\":[\"$ips\"],\"ip_num\":1,\"bandwidth\":\"\"},\"isp2\":{\"ips\":[],\"ip_num\":0,\"bandwidth\":\"\"},\"isp3\":{\"ips\":[],\"ip_num\":0,\"bandwidth\":\"\"},\"isp4\":{\"ips\":[],\"ip_num\":0,\"bandwidth\":\"\"},\"name\":\"vdc$i\",\"description\":\"\",\"auto\":\"false\",\"userid\":\"$userid\",\"vcloudid\":\"$vpcid\",\"poolid\":\"$poolid\",\"vl2s\":{\"subnet1\":{\"name\":\"Subnet1\",\"ip\":\"172.20.0.1\",\"netmask\":\"24\",\"vlantag\":\"\",\"auto\":true,\"vms\":[{\"name\":\"vm$vmname0\",\"os\":\"template-centos\",\"vcpu_num\":\"1\",\"mem_size\":\"512\",\"disk_size\":\"0\",\"ip\":\"172.20.0.$vm0ip\",\"netmask\":\"24\",\"gateway\":\"172.20.0.1\",\"poolid\":\"$vm_poolid\",\"launch_server\":\"$vm_launch_server\"},{\"name\":\"vm$vmname1\",\"os\":\"template-centos\",\"vcpu_num\":\"1\",\"mem_size\":\"512\",\"disk_size\":\"0\",\"ip\":\"172.20.0.$vm1ip\",\"netmask\":\"24\",\"gateway\":\"172.20.0.1\",\"poolid\":\"$vm_poolid\",\"launch_server\":\"$vm_launch_server\"}]},\"subnet2\":{\"name\":\"Subnet2\",\"ip\":\"172.20.1.1\",\"netmask\":\"24\",\"vlantag\":\"\",\"auto\":true,\"vms\":[]},\"subnet3\":{\"name\":\"Subnet3\",\"ip\":\"172.20.2.1\",\"netmask\":\"24\",\"vlantag\":\"\",\"auto\":true,\"vms\":[]}},\"gw_launch_server\":\"$gw_launch_server\"}" > /dev/null 2>&1
    ((vmname1++))
    vmname0=$vmname1
    ((vmname1++))
    ((vm1ip++))
    vm0ip=$vm1ip
    ((vm1ip++))
    ((i++))
done

while [ 1 ]; do
    i=0
    vgwflag=0
    while [ $i -le $vdcnum ]; do
        vdc_state=`$MYSQLEXC "select state from vnet_v2_0 where name=\"vdc$i\""`    
        if [ $vdc_state -eq 0 ]; then
            vdc_errno=`$MYSQLEXC "select errno from vnet_v2_0 where name=\"vdc$i\""`    
            if [ $vdc_errno -eq 0 ]; then
                ((vgwflag++))
            fi
        fi
        ((i++))
    done

    if [ $vgwflag -eq 0 ]; then
        echo created vgws
        date
    fi 

    i=0
    vmflag=0
    while [ $i -le $vmnum ]; do
        vm_state=`$MYSQLEXC "select state from vm_v2_0 where name=\"vm$i\""`    
        if [ $vm_state -eq 0 ]; then
            vm_errno=`$MYSQLEXC "select errno from vm_v2_0 where name=\"vm$i\""`    
            if [ $vm_errno -eq 0 ]; then
                ((vmflag++))
            fi
        fi
        ((i++))
    done

    if [ $vmflag -eq 0 ]; then
        echo created vms
        date
    fi

    if [ $vgwflag -eq 0 -a $vmflag -eq 0 ]; then
        break
    fi

    sleep 2
done

sleep 60

#set user lock
i=0
while [ $i -le $vdcnum ]; do
    userid=`$MYSQLEXC "select id from fdb_user_v2_0 where company=\"yunshan$i\""`
    curl -s -X POST -k https://${dest_ip}/fdbuser/setstate -d "userdata=[{\"userid\":\"$userid\",\"state\":2}]"
    ((i++))
done

#stop vgw
i=0
while [ $i -le $vdcnum ]; do
    vgwid=`$MYSQLEXC "select id from vnet_v2_0 where name=\"vdc$i\""`    
    vdc_state=`$MYSQLEXC "select state from vnet_v2_0 where name=\"vdc$i\""`    
    if [ $vdc_state -eq 7 ]; then
        curl -s -X POST -k https://${dest_ip}/fdbvdc/stopvgw?vgwid=$vgwid > /dev/null 2>&1
    fi
    ((i++))
done

#stop vm
i=0
while [ $i -le $vmnum ]; do
    vmid=`$MYSQLEXC "select id from vm_v2_0 where name=\"vm$i\""`    
    vm_state=`$MYSQLEXC "select state from vm_v2_0 where name=\"vm$i\""`    
    if [ $vm_state -eq 4 ]; then
        curl -s -X POST -k https://${dest_ip}/fdbvm/stopvm?vmid=$vmid > /dev/null 2>&1
    fi
    ((i++))
done

sleep 150

#delete vgw
i=0
while [ $i -le $vdcnum ]; do
    vgwid=`$MYSQLEXC "select id from vnet_v2_0 where name=\"vdc$i\""`    
    curl -s -X POST -k https://${dest_ip}/fdbvdc/deletevgw?vgwid=$vgwid > /dev/null 2>&1
    ((i++))
done

#delete vm
i=0
while [ $i -le $vmnum ]; do
    vmid=`$MYSQLEXC "select id from vm_v2_0 where name=\"vm$i\""`    
    curl -s -X POST -k https://${dest_ip}/fdbvm/deletevm?vmid=$vmid > /dev/null 2>&1
    ((i++))
done

sleep 150

#delete vdc
i=0
while [ $i -le $vdcnum ]; do
    vdcid=`$MYSQLEXC "select id from vnet_v2_0 where name=\"vdc$i\""`    
    curl -s -X POST -k https://${dest_ip}/fdbvdc/deletevdc -d "vdcid=[\"$vdcid\"]" > /dev/null 2>&1
    ((i++))
done

sleep 150

#delete vpc
i=0
while [ $i -le $vdcnum ]; do
    vpcid=`$MYSQLEXC "select id from vcloud_v2_0 where name=\"vpc$i\""`    
    curl -s -X POST -k https://${dest_ip}/fdbvpc/delvpc -d "vpcid=[\"$vpcid\"]" > /dev/null 2>&1
    ((i++))
done

#delete ip
up_ip=90
i=0
while [ $i -le $ipnum ]; do
    curl -s -X POST -k https://${dest_ip}/pool/delip?ip="${up_subnet}.${up_ip}" > /dev/null 
    ((i++))
    ((up_ip++))
done


#delete user
i=0
while [ $i -le $vdcnum ]; do
    userid=`$MYSQLEXC "select id from fdb_user_v2_0 where company=\"yunshan$i\""`    
    curl -s -X POST -k https://${dest_ip}/fdbuser/deluser -d "userid=[\"$userid\"]" > /dev/null 2>&1
    $MYSQLEXC "delete from user_v2_0 where id=$userid"
    ((i++))
done

#done

