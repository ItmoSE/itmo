#!/bin/bash
set -e

# Runtime-каталог FRR
install -d -o frr -g frr -m 775 /run/frr

# На случай повторного запуска контейнера
rm -f /run/frr/*.pid
rm -f /run/frr/*.vty
rm -f /run/frr/zserv.api

# Сначала менеджер маршрутизации
/usr/lib/frr/zebra \
  -d \
  -A 127.0.0.1 \
  -f /etc/frr/zebra.conf

# Даём zebra создать сокет для остальных демонов
for i in $(seq 1 20); do
  if pgrep -x zebra >/dev/null 2>&1; then
    break
  fi
  sleep 0.1
done

if ! pgrep -x zebra >/dev/null; then
  echo "ERROR: zebra failed to start"
  exit 1
fi

# Затем OSPFv2
/usr/lib/frr/ospfd \
  -d \
  -A 127.0.0.1 \
  -f /etc/frr/ospfd.conf

sleep 0.5

if ! pgrep -x ospfd >/dev/null; then
  echo "ERROR: ospfd failed to start"
  exit 1
fi

echo "FRR ready: zebra and ospfd are running"

exec sleep infinity
