################################################################################################
# Project: DeepFive
# Author: Hongzhe Xie
# Date: November 2025
# Description: 
#    CSE 030 Data Structure Project - DeepFive
#    University of California, Merced
################################################################################################

# ----------------------------------------------------------------------------------------------
# Step 1: Use Debian 12 (Bookworm) as the base image
# ----------------------------------------------------------------------------------------------
FROM debian:bookworm

# ----------------------------------------------------------------------------------------------
# Step 2: Install all necessary build tools, libraries, and Xpra dependencies
# ----------------------------------------------------------------------------------------------
RUN apt-get update && apt-get install -y \
    build-essential \
    g++ \
    clang \
    cmake \
    fltk1.3-dev \
    libgl1-mesa-dev \
    libglu1-mesa-dev \
    libx11-dev \
    libpng-dev \
    xpra \
    wget \
    xauth \
    x11-xserver-utils \
    xvfb \
    && rm -rf /var/lib/apt/lists/*

# ----------------------------------------------------------------------------------------------
# Step 3: Configure environment variables
# ----------------------------------------------------------------------------------------------
ENV DISPLAY=:100

# ----------------------------------------------------------------------------------------------
# Step 4: Set up the application directory
# ----------------------------------------------------------------------------------------------
WORKDIR /app

# Copy all project files into the container
COPY . .


# ----------------------------------------------------------------------------------------------
# Step 5: Build the application
# ----------------------------------------------------------------------------------------------
RUN make all


# ----------------------------------------------------------------------------------------------
# Step 6: Launch Xpra server and start the application
# ----------------------------------------------------------------------------------------------
# Bind the Xpra server strictly to localhost on port 8964
CMD xpra start :100 \
    --bind-tcp=0.0.0.0:8964 \
    --start-child=./bin/app \
    --exit-with-children \
    --daemon=no