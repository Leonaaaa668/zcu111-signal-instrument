echo "ZCU111 SD FIT boot"
if fatload mmc 0:1 ${kernel_addr_r} image.ub; then
    bootm ${kernel_addr_r}
fi
