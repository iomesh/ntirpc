define dump_leaf
  set $f_node_off = 0x430
  set $xp_remote_off = 0xf0

  set $idx = $arg0
  set $addr = $arg1
  set $xbase = $addr - $f_node_off
  set $xp_remote_addr = $xbase + $xp_remote_off

  set $family = *(unsigned short *)$xp_remote_addr
  printf "idx=%d ss_family=%d\n", $idx, $family

  if ($family == 10)
    set $port_n = *(unsigned short *)($xp_remote_addr+ 2)
    set $port = (($port_n & 0xff) << 8) | (($port_n >> 8) & 0xff)
    printf "  IPv6 port=%d", $port

    set $b0 = *(unsigned char *)($xp_remote_addr + 8)
    set $b1 = *(unsigned char *)($xp_remote_addr + 9)
    set $b2 = *(unsigned char *)($xp_remote_addr + 10)
    set $b3 = *(unsigned char *)($xp_remote_addr + 11)
    set $b4 = *(unsigned char *)($xp_remote_addr + 12)
    set $b5 = *(unsigned char *)($xp_remote_addr + 13)
    set $b6 = *(unsigned char *)($xp_remote_addr + 14)
    set $b7 = *(unsigned char *)($xp_remote_addr + 15)
    set $b8 = *(unsigned char *)($xp_remote_addr + 16)
    set $b9 = *(unsigned char *)($xp_remote_addr + 17)
    set $b10 = *(unsigned char *)($xp_remote_addr + 18)
    set $b11 = *(unsigned char *)($xp_remote_addr + 19)
    set $b12 = *(unsigned char *)($xp_remote_addr + 20)
    set $b13 = *(unsigned char *)($xp_remote_addr + 21)
    set $b14 = *(unsigned char *)($xp_remote_addr + 22)
    set $b15 = *(unsigned char *)($xp_remote_addr + 23)

    if ($b10 == 0xff && $b11 == 0xff)
      printf "  addr=[::ffff:%d.%d.%d.%d]\n", $b12, $b13, $b14, $b15
    else
      printf "  addr=%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x\n", \
        $b0,$b1,$b2,$b3,$b4,$b5,$b6,$b7,$b8,$b9,$b10,$b11,$b12,$b13,$b14,$b15
    end
  end
end

define dump_rebree_x_part
  set $svc_fd_xpart_addr = $arg0
  set $npart = *(int*)($svc_fd_xpart_addr + 0x28)
  set $rbtree_x_part_base = *(void**)($svc_fd_xpart_addr + 0x38)
  set $rbtree_x_part_size = 0x120
  set $rbtree_node_count_off = 0xc8
  set $rbtree_root_off = 0xb8

  set $i = 0
  while ($i < $npart)
    set $rbtree_x_part_addr = $rbtree_x_part_base + ($i * $rbtree_x_part_size)
    set $num = *(unsigned long *)($rbtree_x_part_addr + $rbtree_node_count_off)

    if ($num == 1)
      set $root_addr = *(void **)($rbtree_x_part_addr + $rbtree_root_off)
      dump_leaf $i $root_addr
    end

    set $i = $i + 1
  end
end
