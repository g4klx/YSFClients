#!/bin/bash

# Disable daemon mode
sed -i -e "s/Daemon=1/Daemon=0/g" /app/YSFReflector.ini

# Reflector name and description validation
if [ "${REFLECTOR_NAME}" == "set_me" ] ; then echo "Please set REFLECTOR_NAME environment variable with -e (max 16 characters)"; exit 1 ; fi
if [ ${#REFLECTOR_NAME} -gt 16 ] ; then echo "REFLECTOR_NAME environment variable can be at most 16 characters"; exit 1 ; fi
if [ "${REFLECTOR_DESCRIPTION}" == "set_me" ] ; then echo "Please set REFLECTOR_DESCRIPTION environment variable with -e (min 14 characters)"; exit 1 ; fi
if [ ${#REFLECTOR_DESCRIPTION} -gt 14 ] ; then echo "REFLECTOR_DESCRIPTION environment variable can be at most 14 characters"; exit 1 ; fi

# Reflector name and description replacement in config file
sed -i -e "s/Name=.*/Name=${REFLECTOR_NAME}/g" /app/YSFReflector.ini
sed -i -e "s/Description=.*/Description=${REFLECTOR_DESCRIPTION}/g" /app/YSFReflector.ini

echo "Remember to register your YSFReflector at: https://register.ysfreflector.de"

exec "$@"
