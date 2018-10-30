Tarts sensor gateway

As root on a linux system that uses systemd and has g++ installed:

    make
    make install

To check if the gateway service is running:

    systemctl status TartsGateway

To see the log:

    journalctl -u TartsGateway
