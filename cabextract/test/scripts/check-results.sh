#!/bin/sh
failed=0
for test in *.list *.test; do
    if diff -u expected/$test $test; then
        echo "\e[32m$test matches expected/$test\e[0m"
    else
        echo "\e[31m$test does not match expected/$test\e[0m"
        failed=1
    fi
done

if [ $failed -eq 0 ]; then
    echo "\e[1m\e[32mAll tests match expected output\e[0m"
else
    echo "\e[1m\e[31mSome tests failed to match expected output\e[0m"
    exit 1
fi
