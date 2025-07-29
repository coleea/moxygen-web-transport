

이 프로젝트는 unix 계열 os에서 작동하며 windows 환경에서는 작동하지 않는다. 그러므로 windows  이용자는 wsl2를 이용하여 실행하도록 한다 

## 실행방법  (windows 11 WSL2)

참고 : https://github.com/facebookexperimental/moq-encoder-player/issues/24


0. 아래 프로그램을 설치한다 

```
sudo apt install ffmpeg
```

1. encoder-player 서버를 실행한다  (1번째 터미널에서 실행할 것)

```
cd ./moq-encoder-player
./create_self_signed_certs.sh // ssh certicifate 를 생성한다
./start-http-server-cross-origin-isolated.py // python 서버를 구동한다
```


2. media over quic transport relay 서버를 빌드한다

```
cd moxygen
./build.sh
```

3.  media over quic transport relay 서버를 실행한다 (2번째 터미널에서 실행할 것)

// moxygen 폴더로 이동한다
```
 ./_build/bin/moqrelayserver -port 4433 -cert ../moq-encoder-player/certs/certificate.pem  -key ../moq-encoder-player/certs/certificate.key  --logging DBG1 --endpoint /moq

```

4. 실시간 라이브 스트리밍 데이터를 생성한다 (3번째 터미널에서 실행할 것)

```
rm ~/Movies/fifo.flv
mkdir ~/Movies
mkfifo ~/Movies/fifo.flv
ffmpeg -y -f lavfi -re -i smptebars=duration=300:size=320x200:rate=30 -f lavfi -re -i sine=frequency=1000:duration=300:sample_rate=48000 -pix_fmt yuv420p -c:v libx264 -b:v 180k -g 60 -keyint_min 60 -profile:v baseline -preset veryfast -c:a aac -b:a 96k -vf "drawtext=fontfile=/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf: text=\'Local time %{localtime\: %Y\/%m\/%d %H.%M.%S} (%{n})\': x=10: y=10: fontsize=12: fontcolor=white: box=1: boxcolor=0x00000099" -f flv ~/Movies/fifo.flv
```

ffmpeg 명령어 분석 : 실시간으로 SMPTE 컬러 바(Color Bars) 비디오와 1000Hz의 사인파(sine wave) 오디오를 생성하고, 비디오 위에 현재 시간을 표시하여 FLV(Flash Video) 형식으로 파일에 저장하는 기능을 수행합니다

주로 라이브 스트리밍 테스트나 디버깅 목적으로 사용된다


### Argument 별 상세 설명

| Argument | 설명 |
|---|---|
| `ffmpeg` | 비디오 및 오디오 파일을 처리하기 위한 커맨드 라인 도구입니다. |
| `-y` | 출력 파일이 이미 존재할 경우 덮어쓸 것인지 묻지 않고 바로 덮어씁니다. |
| `-f lavfi` | FFmpeg의 libavfilter 라이브러리를 입력 형식으로 사용하도록 지정합니다. 이를 통해 필터 그래프를 소스로 사용할 수 있습니다. |
| `-re` | 입력 파일을 네이티브 프레임 속도로 읽도록 지시합니다. 라이브 스트리밍을 시뮬레이션할 때 사용됩니다. |
| `-i smptebars=duration=300:size=320x200:rate=30` | SMPTE(Society of Motion Picture and Television Engineers) 표준 컬러 바 패턴을 생성하는 비디오 소스를 입력으로 지정합니다.<ul><li>`duration=300`: 비디오 길이를 300초로 설정합니다.</li><li>`size=320x200`: 비디오 해상도를 320x200 픽셀로 설정합니다.</li><li>`rate=30`: 초당 프레임 수를 30으로 설정합니다.</li></ul> |
| `-i sine=frequency=1000:duration=300:sample_rate=48000` | 특정 주파수의 사인파 오디오를 생성하는 소스를 입력으로 지정합니다.<ul><li>`frequency=1000`: 사인파의 주파수를 1000Hz로 설정합니다.</li><li>`duration=300`: 오디오 길이를 300초로 설정합니다.</li><li>`sample_rate=48000`: 샘플링 속도를 48000Hz로 설정합니다.</li></ul> |
| `-pix_fmt yuv420p` | 출력 비디오의 픽셀 포맷을 `yuv420p`로 설정합니다. 이는 다양한 플레이어 및 장치와의 호환성을 위해 널리 사용되는 형식입니다. |
| `-c:v libx264` | 비디오 코덱으로 H.264 인코더인 `libx264`를 사용하도록 지정합니다. |
| `-b:v 180k` | 비디오 비트레이트를 180kbps로 설정합니다. |
| `-g 60` | GOP(Group of Pictures) 크기를 60으로 설정합니다. 이는 60프레임마다 하나의 키프레임(I-frame)을 생성한다는 의미입니다. |
| `-keyint_min 60` | 최소 키프레임 간격을 60으로 설정합니다. |
| `-profile:v baseline` | H.264 프로파일을 `baseline`으로 설정합니다. 이 프로파일은 낮은 컴퓨팅 성능을 가진 장치와의 호환성을 위해 사용됩니다. |
| `-preset veryfast` | 인코딩 속도와 압축률 간의 균형을 맞추는 프리셋을 `veryfast`로 설정합니다. `veryfast`는 인코딩 속도를 우선시합니다. |
| `-c:a aac` | 오디오 코덱으로 AAC(Advanced Audio Coding)를 사용하도록 지정합니다. |
| `-b:a 96k` | 오디오 비트레이트를 96kbps로 설정합니다. |
| `-vf "drawtext=..."` | 비디오에 텍스트를 오버레이하는 `drawtext` 비디오 필터를 적용합니다.<ul><li>`fontfile=...`: 텍스트에 사용할 폰트 파일의 경로를 지정합니다.</li><li>`text='...'`: 표시할 텍스트의 내용을 지정합니다. `%{localtime\: %Y\/%m\/%d %H.%M.%S}`는 현재 로컬 시간을 "년/월/일 시.분.초" 형식으로 표시하고, `(%{n})`은 현재 프레임 번호를 나타냅니다.</li><li>`x=10: y=10`: 텍스트의 위치를 비디오의 왼쪽 상단 모서리에서 가로 10, 세로 10 픽셀 떨어진 곳으로 지정합니다.</li><li>`fontsize=12`: 폰트 크기를 12로 설정합니다.</li><li>`fontcolor=white`: 폰트 색상을 흰색으로 설정합니다.</li><li>`box=1`: 텍스트 주위에 배경 상자를 표시하도록 설정합니다.</li><li>`boxcolor=0x00000099`: 배경 상자의 색상을 검은색으로, 투명도를 99(16진수)로 설정합니다.</li></ul> |
| `-f flv` | 출력 파일의 형식을 FLV(Flash Video)로 지정합니다. |
| `~/Movies/fifo.flv` | 출력 파일의 경로와 이름을 지정합니다. |

5. FLV Streamer를 실행한다  (4번째 터미널에서 실행할 것)

// moxygen 폴더로 이동한다
```
./_build/bin/moqflvstreamerclient -input_flv_file ~/Movies/fifo.flv --logging DBG4
```

6. web-player 서버에 접속한다 

`http://localhost:8080/src-player` 으로 접속한다 

<!-- - Open chrome with http://localhost:8080/src-player/index.html?local&verbose=1&host=moq://localhost:4433/moq -->

호스트 주소는 moq://localhost:4433/moq 를 적지 않는다. 이유는 wsl2에서 udp로 패킷을 전송할 때 localhost 대신 ip 어드레스를 직접 적어줘야 하는데 자세한 이유는 본인도 모른다. 

아래 명령어로 WSL2의 아이피 주소를 알아낸다

```
ip addr show eth0 | grep -oP '(?<=inet\s)\d+(\.\d+){3}'
```

가령 192.168.82.150 이 표시되었다면 이 서버의 url은 `localhost:4433/moq` 대신 `192.168.82.150/moq` 를 사용하도록 한다


- Set Namespace to flvstreamer
- Remove trackname
- Press start

시작 버튼을 누르면 아래 에러가 표시된다

```
Failed to establish a connection to https://192.168.82.150/moq: net::ERR_QUIC_PROTOCOL_ERROR.QUIC_NETWORK_IDLE_TIMEOUT (No recent network activity after 4007214us. Timeout:4s num_undecryptable_packets: 0 {}).
```

