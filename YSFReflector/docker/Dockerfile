FROM debian:buster-slim AS builder

RUN apt-get update && \
    apt-get install -y build-essential git

RUN mkdir /code && \
    git clone https://github.com/g4klx/YSFClients.git /code && \
    cd /code/YSFReflector/ && \
    make clean all

FROM debian:buster-slim

ENV REFLECTOR_NAME set_me
ENV REFLECTOR_DESCRIPTION set_me

RUN apt-get update && \
    apt-get install -y procps && \
    rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/* && \
    mkdir /app

COPY --from=builder /code/YSFReflector/YSFReflector.ini /app/YSFReflector.ini
COPY --from=builder /code/YSFReflector/YSFReflector /app/YSFReflector
COPY entrypoint.sh /entrypoint.sh

EXPOSE 42000/udp

ENTRYPOINT ["/entrypoint.sh"]
CMD ["/app/YSFReflector", "/app/YSFReflector.ini"]
HEALTHCHECK CMD ps aux | grep [Y]SFReflector || exit 1
