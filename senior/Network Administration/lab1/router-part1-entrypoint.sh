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
    ip route add 10.0.1.3/32 via 10.0.13.3
    ip route add default via 10.0.12.2
    ;;
  R2)
    ip addr add 10.0.12.2/24 dev eth1
    ip addr add 10.0.23.2/24 dev eth2
    ip addr add 10.0.1.2/32 dev lo
    ip route add 10.0.1.1/32 via 10.0.12.1 metric 60
    ip route add 10.0.1.1/32 via 10.0.23.3 metric 100
    ip route add 10.0.1.3/32 via 10.0.23.3
    ;;
  R3)
    ip addr add 10.0.13.3/24 dev eth1
    ip addr add 10.0.23.3/24 dev eth2
    ip addr add 10.0.1.3/32 dev lo
    ip route add 10.0.1.1/32 via 10.0.13.1
    ip route add 10.0.1.2/32 via 10.0.23.2
    ;;
  *)
    echo "Unsupported ROUTER_NAME=${ROUTER_NAME}" >&2
    exit 1
    ;;
esac

echo "${ROUTER_NAME}: final state of part 1 is ready"
exec sleep infinity
