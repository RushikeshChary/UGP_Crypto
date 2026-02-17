To run this supersonic OT, one has to make sure that normal mpc operations are working correctly.\
Then, run the following command to compile the supersonic_ot.cpp file.
```
g++ -std=gnu++20 \
    -O2 \
    -Wall -Wextra \
    -pthread \
    -mavx -mavx2 \
    supersonic_ot.cpp \
    -o supersonic_ot_test \
    -lboost_system
```
To run the parties:
```
./supersonic_ot_test <p0|p1|p2>
```

