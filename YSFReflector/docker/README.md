# YSFReflector Docker Image

The `Dockerfile` here is intended to produce an image which will be stored on [Docker Hub](https://hub.docker.com/).

# Requirements

* [Docker](https://docs.docker.com/install/)
* Firewall setup to accept 42000 (UDP)

# Usage

`docker run -e REFLECTOR_NAME=YOUR_NAME_HERE -eREFLECTOR_DESCRIPTION=YOUR_DESCRIPTION_HERE -p 42000:42000/udp neilbartley/ysfreflector:latest`

# How to build

Building isn't required unless you need a newer image or have made changes.

```
cd YSFReflector/docker
YSF_TAG=$(date +'%Y%m%d')-$(git rev-parse --short HEAD)
docker build --rm -t neilbartley/ysfreflector:$YSF_TAG .
docker tag neilbartley/ysfreflector:$YSF_TAG neilbartley/ysfreflector:latest
docker push neilbartley/ysfreflector:$YSF_TAG
docker push neilbartley/ysfreflector:latest
```
