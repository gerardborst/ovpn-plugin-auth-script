#!/bin/bash
set -ex
openvpn_version=2.5.8

#curl https://swupdate.openvpn.org/community/releases/openvpn-${openvpn_version}.tar.gz | tar xzC /tmp openvpn-${openvpn_version}/include/openvpn-plugin.h

#cp /tmp/openvpn-${openvpn_version}/include/openvpn-plugin.h /usr/include/

make clean all

