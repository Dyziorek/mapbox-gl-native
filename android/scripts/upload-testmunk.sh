#!/usr/bin/env bash

set -e
set -o pipefail

EMAIL="leith@mapbox.com"
APP_NAME="Mapbox"
APK_PATH=$1

echo "submitting testrun to testmunk..."

zip -r android/test/features.zip android/test/features/

RESPONSE=$(curl -f -H "Accept: application/vnd.testmunk.v1+json" -F "file=@${APK_PATH}" -F "testcases=@android/test/features.zip" -F "email=${EMAIL}" -F "autoStart=true" -F "public=true" -F "devices=lg-nexus-5-A" "https://${TESTMUNK_KEY}@api.testmunk.com/apps/${APP_NAME}/testruns")

TESTRUN_ID=$(echo "${RESPONSE}" | jq -r '.id')
TESTRUN_NAME=$(echo "${RESPONSE}" | jq -r '.name')

echo "successully uploaded to Testmunk as '${TESTRUN_NAME}'"
echo "public link to test results: https://www.testmunk.com/testrun/${TESTRUN_ID}"

echo "waiting for test results..."

STATUS="Waiting"

until [ "$STATUS" != "Waiting" ] && [ "$STATUS" != "Running" ]; do
    RESPONSE=$(curl -X GET -H "Accept: application/vnd.testmunk.v1+json" –sS "https://${TESTMUNK_KEY}@api.testmunk.com/apps/${APP_NAME}/testruns/${TESTRUN_ID}")
    STATUS=$(echo "${RESPONSE}" | jq -r '.status')
    echo "test status is ${STATUS}"
    sleep 10
done

if [[ "$STATUS" == "Success" ]]; then
    echo "TESTS PASSED"
    exit 0
else
    echo "TESTS FAILED"
    exit 1
fi
