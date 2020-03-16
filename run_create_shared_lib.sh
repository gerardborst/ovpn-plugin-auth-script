#!/bin/bash
set -ex

openvpn_version=2.5.8

save_dir=$PWD

SCRIPT_DIR=$(dirname $0)
cd ${SCRIPT_DIR}
SCRIPT_DIR=$PWD

#curl https://swupdate.openvpn.org/community/releases/openvpn-${openvpn_version}.tar.gz | tar xzC /tmp openvpn-${openvpn_version}/include/openvpn-plugin.h

docker run -ti -v /tmp/openvpn-${openvpn_version}/include/openvpn-plugin.h:/usr/include/openvpn-plugin.h -v $PWD:/build --rm -w /build --user 1000 gcc:9.5.0-buster ./create_shared_lib.sh

cd ${save_dir}
