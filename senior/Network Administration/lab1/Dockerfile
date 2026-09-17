FROM ubuntu:24.04

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y \
    iproute2 \
    iputils-ping \
    traceroute \
    procps \
    tcpdump \
    frr \
    tini \
 && rm -rf /var/lib/apt/lists/* \
 && install -o frr -g frrvty -m 660 /dev/null /etc/frr/zebra.conf \
 && install -o frr -g frrvty -m 660 /dev/null /etc/frr/ospfd.conf

COPY router-entrypoint.sh /usr/local/bin/router-entrypoint.sh

RUN chmod +x /usr/local/bin/router-entrypoint.sh

ENTRYPOINT ["/usr/bin/tini", "--"]
CMD ["/usr/local/bin/router-entrypoint.sh"]
