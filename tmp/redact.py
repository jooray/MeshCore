#!/usr/bin/env python3
import sys

prev_line = None
count = 0

for line in sys.stdin:
    line = line.rstrip('\n')
    
    if line == prev_line:
        count += 1
    else:
        # Output previous line if exists
        if prev_line is not None:
            print(prev_line)
            if count > 1:
                print(f"[repeats {count} times]")
        
        # Start tracking new line
        prev_line = line
        count = 1

# Output the last line
if prev_line is not None:
    print(prev_line)
    if count > 1:
        print(f"[repeats {count} times]")
