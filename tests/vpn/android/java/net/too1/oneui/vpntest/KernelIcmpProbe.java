package net.too1.oneui.vpntest;

import android.net.Network;
import android.system.Os;
import android.system.OsConstants;
import android.system.StructTimeval;
import java.io.FileDescriptor;
import java.net.InetAddress;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;

/** Test-only OS ping socket: traverses the selected VPN, never injects TUN packets. */
public final class KernelIcmpProbe {
    private KernelIcmpProbe() {}

    public static boolean echo(Network network, String ipv4, int sequence) throws Exception {
        // Targets come from the consumer's authorised peer inventory, not shell commands.
        if (network == null || ipv4 == null || !ipv4.matches("[0-9]{1,3}(\\.[0-9]{1,3}){3}"))
            throw new IllegalArgumentException("An IPv4 peer and VPN network are required");
        byte[] expected = ("OneUI kernel ICMP " + sequence).getBytes(StandardCharsets.UTF_8);
        byte[] packet = new byte[8 + expected.length];
        packet[0] = 8;
        packet[6] = (byte)(sequence >> 8);
        packet[7] = (byte)sequence;
        System.arraycopy(expected, 0, packet, 8, expected.length);
        int sum = 0;
        for (int i = 0; i < packet.length; i += 2)
            sum += ((packet[i] & 255) << 8) | (i + 1 < packet.length ? packet[i + 1] & 255 : 0);
        while ((sum >>> 16) != 0)
            sum = (sum & 65535) + (sum >>> 16);
        int checksum = ~sum & 65535;
        packet[2] = (byte)(checksum >> 8);
        packet[3] = (byte)checksum;
        FileDescriptor socket =
            Os.socket(OsConstants.AF_INET, OsConstants.SOCK_DGRAM, OsConstants.IPPROTO_ICMP);
        try {
            network.bindSocket(socket);
            Os.setsockoptTimeval(socket, OsConstants.SOL_SOCKET, OsConstants.SO_RCVTIMEO,
                                 StructTimeval.fromMillis(1500));
            Os.connect(socket, InetAddress.getByName(ipv4), 0);
            if (Os.write(socket, packet, 0, packet.length) != packet.length)
                return false;
            byte[] response = new byte[512];
            int length = Os.read(socket, response, 0, response.length);
            return length == packet.length && response[0] == 0 &&
                Arrays.equals(expected, Arrays.copyOfRange(response, 8, length));
        } finally {
            Os.close(socket);
        }
    }
}
