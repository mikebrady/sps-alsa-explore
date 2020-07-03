FROM alpine AS builder

# This may be modified by the Github Action Workflow.
ARG ALSAEXPLORE_BRANCH=master

RUN apk -U add \
        git \
        build-base \
        autoconf \
        automake \
        alsa-lib-dev
 

RUN 	git clone https://github.com/mikebrady/alsaexplore
WORKDIR alsaexplore
RUN 	git checkout "$ALSAEXPLORE_BRANCH"
RUN 	autoreconf -fi
RUN 	./configure
RUN 	make

# Runtime
FROM alpine
RUN 	apk add alsa-lib 
RUN 	rm -rf  /lib/apk/db/*

COPY 	--from=builder /alsaexplore/alsaexplore /usr/local/bin

ENTRYPOINT [ "/bin/sh" ]

