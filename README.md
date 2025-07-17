# 项目环境配置
## 云服务器和docker安装
1. 购买云服务器(这里推荐阿里云，便宜还好用，火山云和华为云更便宜，特别是华为云)
2. 在云服务器上安装gcc和git这些基础开发工具，git要配置ssh key，网上有教程，这里不再赘述。
3. 安装docker，阿里云的docker安装不会有那个AI助手，直接问指令就行，一般最容易遇到的问题就是docker源被墙了，连接不到dockerhub。
4. 拉取项目代码，创建一个文件夹放代码。

```shell
mkdir work
cd work
git clone git@github.com:Summus1999/safeUdp.git
```

5. 用户加入docker组，然后验证一下docker组是否加入成功

```shell
sudo groupadd docker
sudo usermod -aG docker ${USER}
sudo systemctl restart docker 
newgrp docker
docker ps
#重启一下机器，全局生效
sudo docker -v
```

6. 创建docker镜像,进入到对应的目录构建镜像

```shell
## 进入对应目录
cd work/safeUdp/ docker/build
## 启动构建镜像
docker build --network host -t  safe:udp -f safe-udp.dockerfile . 
#查看镜像id
docker images 
```

7. 进入脚本目录，运行对应的脚本文件

```shell
## 进入脚本目录
cd ~/work/safeUdp/docker/scripts
#启动容器
./safeudp_docker_run.sh
#进入容器
./safeudp_docker_into.sh
```

8. 编译代码

```shell
cd /work
mkdir build
cd build/
cmake ..
## 开6线程进行编译，如果云服务器或者本地虚拟机配置好可以开更多，如果配置不太行开少点也OK
make -j6
make install
```

9. 运行程序

```shell
## 运行server端
#format:  <server-port> <receiver-window>
cd /work/build/bin
./server 8081 100
```

10. 运行client端

```shell
cd ~/work/safe-udp/docker/scripts
#进入容器
./safeudp_docker_into.sh
cd /work/build/bin
#format: <server-ip> <server-port> <file-name> <receiver-window> <control-param> <drop/delay%>
./client localhost 8081 天龙八部.txt 100  0 0
```

11. 运行结果查看

```shell
## 验证收发双方的文本内容是否一致
bash /work/diff.sh 天龙八部.txt
```
