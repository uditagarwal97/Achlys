# Triggers

1. Michael Dunphy: argv[1] has to be 0. Run as `./michael_dunphy 0`
2. Peter Eastman: argv[1]...argv[5] have to be < 15. Run as `./peter_eastman 12 13 7 6 10`
3. Stephen Dolan: argv[1] should be > 10 digits. Run as `./stephen_dollan 10470264036`  
This returns you a square root of 0, which is not a NaN, but can cause a NaN  
4. Philipp Klenze: At least one of argv[1]...argv[5] should be close to 2000 (> 1860). Run as `./philipp_klenze 1890 402 1525 1970 173`
5. Ghislain Lemaur: Input from(in this case output to) a file. The default path is /tmp/log.txt. If the UID is > 10000, it creates a bug. Can achieve it by `usermod -u 10001 <USER> && chown <USER> /tmp/log.txt`
6. Matt Bierner: All of argv[1]...argv[5] should be less than reference (100). Run as `./matt_bierner 42 15 63 93 91`  
This might cause a Segmentation Fault instead of a NaN depending on the System  
 
