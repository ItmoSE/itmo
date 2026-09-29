#!/bin/bash
set -euo pipefail

image_name=network-lab-part1:latest
containers=(na-part1-r1 na-part1-r2 na-part1-r3)

docker build -f Dockerfile.part1 -t "${image_name}" .
docker rm -f "${containers[@]}" >/dev/null 2>&1 || true

for index in 1 2 3; do
  docker run -d \
    --name "na-part1-r${index}" \
    --hostname "R${index}" \
    --network none \
    --cap-add NET_ADMIN \
    --cap-add NET_RAW \
    --sysctl net.ipv4.ip_forward=1 \
    -e "ROUTER_NAME=R${index}" \
    "${image_name}" >/dev/null
done

pid_r1=$(docker inspect -f '{{.State.Pid}}' na-part1-r1)
pid_r2=$(docker inspect -f '{{.State.Pid}}' na-part1-r2)
pid_r3=$(docker inspect -f '{{.State.Pid}}' na-part1-r3)

docker run --rm --privileged --network host --pid host \
  --entrypoint /bin/bash "${image_name}" -lc "
    ip link add p1r1r2 type veth peer name p1r2r1
    ip link add p1r1r3 type veth peer name p1r3r1
    ip link add p1r2r3 type veth peer name p1r3r2
    ip link set p1r1r2 netns ${pid_r1}
    ip link set p1r2r1 netns ${pid_r2}
    ip link set p1r1r3 netns ${pid_r1}
    ip link set p1r3r1 netns ${pid_r3}
    ip link set p1r2r3 netns ${pid_r2}
    ip link set p1r3r2 netns ${pid_r3}
  "

docker exec na-part1-r1 ip link set p1r1r2 name eth1
docker exec na-part1-r1 ip link set p1r1r3 name eth2
docker exec na-part1-r2 ip link set p1r2r1 name eth1
docker exec na-part1-r2 ip link set p1r2r3 name eth2
docker exec na-part1-r3 ip link set p1r3r1 name eth1
docker exec na-part1-r3 ip link set p1r3r2 name eth2

for attempt in $(seq 1 100); do
  if docker logs na-part1-r1 2>&1 | grep -q 'final state' && \
     docker logs na-part1-r2 2>&1 | grep -q 'final state' && \
     docker logs na-part1-r3 2>&1 | grep -q 'final state'; then
    break
  fi
  sleep 0.1
done

for container_name in "${containers[@]}"; do
  if ! docker logs "${container_name}" 2>&1 | grep -q 'final state'; then
    echo "${container_name} did not reach the final state" >&2
    docker logs "${container_name}" >&2
    exit 1
  fi
done

echo 'Part 1 is ready. Route tables:'
for index in 1 2 3; do
  echo "--- R${index} ---"
  docker exec "na-part1-r${index}" ip route show
done

echo 'Check: docker exec na-part1-r1 ping -c 3 -I 10.0.1.1 10.0.1.2'
