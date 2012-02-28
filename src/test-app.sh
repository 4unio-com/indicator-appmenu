#!/bin/sh

dbus-test-runner --max-wait 0 -b test-app.bustle --task ./test-run-loader.sh --task $1
