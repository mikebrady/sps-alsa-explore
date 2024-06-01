FROM alpine AS builder

# Check required arguments exist. These will be provided by the Github Action
# Workflow and are required to ensure the correct branches are being used.
ARG SPS_ALSA_EXPLORE_BRANCH
RUN test -n "$SPS_ALSA_EXPLORE_BRANCH"

RUN apk -U add \
        autoconf \
        automake \
        build-base \
        git \
        alsa-lib-dev

##### Build  #####
WORKDIR /sps-alsa-explore
COPY . .
RUN git checkout "$SPS_ALSA_EXPLORE_BRANCH"
RUN autoreconf -i 
RUN ./configure
RUN make
WORKDIR /
##### Built #####

# sps-alsa-explore runtime
FROM alpine

RUN apk -U add \
        alsa-lib

# Copy build files.
COPY --from=builder /sps-alsa-explore/sps-alsa-explore /

# Remove anything we don't need.
RUN rm -rf /lib/apk/db/*

Entrypoint ["/sps-alsa-explore"]