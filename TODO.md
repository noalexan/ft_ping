[14:40:47] noah:ft_ping git:(go-crazy*) $ sudo ./ft_ping --count=3 --size=1024 google.com 1.1.1.1
PING google.com (142.250.201.174): 1024 data bytes
1032 bytes from 142.250.201.174: icmp_seq=0 ttl=116 time=26.348 ms
--- google.com ping statistics ---
3 packets transmitted, 1 packets received, 66% packet loss


[14:45:20] noah:ft_ping git:(go-crazy*) $ sudo ./ft_ping --count=3 localhost
PING localhost (127.0.0.1): 56 data bytes
64 bytes from 127.0.0.1: icmp_seq=0 ttl=64 time=0.012 ms
--- localhost ping statistics ---
3 packets transmitted, 1 packets received, 66% packet loss
