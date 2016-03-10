# svchax

ARM11 kernel exploit up to system version 10.6.0-31 (kver 2.50-11) using memchunkhax 1 and 2.

this exploit will only grant access to all svc's for the main thread, while trying to have minimal side effects on the system.
success rate on kver > 2.46-0 is still low.

access to privileged services or modifying the process's svc access control can be obtained if needed by using svcBackdoor. (see https://github.com/Myriachan/libkhax or https://github.com/Steveice10/memchunkhax2)