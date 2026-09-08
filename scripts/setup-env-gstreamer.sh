ln -snf /usr/share/zoneinfo/Asia/Ho_Chi_Minh /etc/localtime && echo Asia/Ho_Chi_Minh > /etc/timezone
apt-get update -y

apt install x11-apps -y

# Gstreamer
apt-get install -y \
    libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev libgstreamer-plugins-bad1.0-dev gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly gstreamer1.0-libav gstreamer1.0-tools gstreamer1.0-x gstreamer1.0-alsa gstreamer1.0-gl gstreamer1.0-gtk3 gstreamer1.0-qt5 gstreamer1.0-pulseaudio \
    alsa pulseaudio

# sudo apt-get install -y --reinstall gstreamer1.0-plugins-ugly libx264-163

# apt-get install -y --reinstall \
#     libavcodec58 libavutil56 libvpx7 libx265-199 libmpg123-0 \
#     libde265-0 libmpeg2-4 libmpeg2encpp-2.1-0 \
#     libx264-163 gstreamer1.0-plugins-ugly gstreamer1.0-libav gstreamer1.0-plugins-bad libfaad2

# # Disable unused Rivermax plugin if present to prevent librivermax.so.1 warning
# if [ -f /opt/nvidia/deepstream/deepstream*/lib/gst-plugins/libnvdsgst_udp.so ]; then
#     mv /opt/nvidia/deepstream/deepstream*/lib/gst-plugins/libnvdsgst_udp.so /opt/nvidia/deepstream/deepstream*/lib/gst-plugins/libnvdsgst_udp.so.bak 2>/dev/null || true
# fi

# # Refresh GStreamer registry cache
# rm -rf ~/.cache/gstreamer-1.0/


# aplay /usr/share/sounds/alsa/Front_Center.wav
# gst-launch-1.0 playbin uri=file:///usr/share/sounds/alsa/Front_Center.wav