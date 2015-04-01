#!/usr/bin/env bash

set -e
set -o pipefail
set -u

xcodebuild test \
    -project ./test/ios/ios-tests.xcodeproj \
    -scheme 'Mapbox GL Tests' \
    -destination 'platform=iOS Simulator,name=iPhone 5s,OS=latest' \
    -destination-timeout 1
