#!/bin/bash
set -euo pipefail

: "${ROUTER_NAME:?ROUTER_NAME must be R1, R2 or R3}"

for interface_name in eth1 eth2; do
  until ip link show "${interface_name}" >/dev/null 2>&1; do
    sleep 0.1
  done
  ip link set "${interface_name}" up
done

case "${ROUTER_NAME}" in
  R1)
    ip addr add 10.0.12.1/24 dev eth1
    ip addr add 10.0.13.1/24 dev eth2
    ip addr add 10.0.1.1/32 dev lo
    router_id=10.0.1.1
    network_a=10.0.12.0/24
    network_b=10.0.13.0/24
    ;;
  R2)
    ip addr add 10.0.12.2/24 dev eth1
    ip addr add 10.0.23.2/24 dev eth2
    ip addr add 10.0.1.2/32 dev lo
    router_id=10.0.1.2
    network_a=10.0.12.0/24
    network_b=10.0.23.0/24
    ;;
  R3)
    ip addr add 10.0.13.3/24 dev eth1
    ip addr add 10.0.23.3/24 dev eth2
    ip addr add 10.0.1.3/32 dev lo
    router_id=10.0.1.3
    network_a=10.0.13.0/24
    network_b=10.0.23.0/24
    ;;
  *)
    echo "Unsupported ROUTER_NAME=${ROUTER_NAME}" >&2
    exit 1
    ;;
esac

install -d -o frr -g frr -m 775 /run/frr
rm -f /run/frr/*.pid /run/frr/*.vty /run/frr/zserv.api

/usr/lib/frr/zebra -d -A 127.0.0.1 -f /etc/frr/zebra.conf
/usr/lib/frr/ospfd -d -A 127.0.0.1 -f /etc/frr/ospfd.conf

for attempt in $(seq 1 50); do
  if vtysh -c "show version" >/dev/null 2>&1; then
    break
  fi
  sleep 0.1
done

vtysh \
  -c "configure terminal" \
  -c "router ospf" \
  -c "ospf router-id ${router_id}" \
  -c "network ${router_id}/32 area 0.0.0.0" \
  -c "network ${network_a} area 0.0.0.0" \
  -c "network ${network_b} area 0.0.0.0"

if [[ "${ROUTER_NAME}" == "R3" ]]; then
  vtysh \
    -c "configure terminal" \
    -c "router ospf" \
    -c "area 0.0.0.0 authentication message-digest" \
    -c "interface eth1" \
    -c "ip ospf message-digest-key 1 md5 HCIA-Datacom" \
    -c "interface eth2" \
    -c "ip ospf message-digest-key 1 md5 HCIA-Datacom"
else
  vtysh \
    -c "configure terminal" \
    -c "interface eth1" \
    -c "ip ospf authentication message-digest" \
    -c "ip ospf message-digest-key 1 md5 HCIA-Datacom" \
    -c "interface eth2" \
    -c "ip ospf authentication message-digest" \
    -c "ip ospf message-digest-key 1 md5 HCIA-Datacom"
fi

if [[ "${ROUTER_NAME}" == "R1" ]]; then
  vtysh \
    -c "configure terminal" \
    -c "router ospf" \
    -c "default-information originate always" \
    -c "interface eth1" \
    -c "ip ospf cost 30"
fi

echo "${ROUTER_NAME}: final state of part 2 is ready"
exec sleep infinity
