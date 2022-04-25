#!/usr/bin/env bash

#
# prep-keys
#
# Prepare remote debugging keys
#
# @author    Tony Smith
# @copyright 2022, Twilio
# @version   1.0.0
# @license   MIT
#

# GLOBALS
key_name="remote-debug"
key_tail="-key.pem"

# RUNTIME START
if [[ ! -f "${key_name}-public${key_tail}" ]]; then
    # Make the keys
        echo "Creating new keys"
    openssl ecparam -name prime256v1 -genkey -noout -out "${key_name}-private${key_tail}"
    openssl ec -in "${key_name}-private${key_tail}" -pubout -out "${key_name}-public${key_tail}"
else
    echo "Using existing keys"
fi
