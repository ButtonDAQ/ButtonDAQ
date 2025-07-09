#!/bin/bash                                                                                                                                                                                                                                                                                                                                                            
cd /opt/ButtonDAQ
source Setup.sh
./NodeDaemon RBU01 </dev/null &>/dev/null
