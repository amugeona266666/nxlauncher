# 게임 런처 (nxlauncher)

모든 닌텐도 기종의 게임을 한 목록에서 고르고, 맞는 에뮬레이터로 바로 실행하는 스위치 홈브루.

## 기능
- 공용 롬 폴더 `sdmc:/roms/<기종>/` 하나로 전 기종 관리 (하위 폴더 1단계까지 인식)
- 기종별 에뮬레이터 자동 선택: RetroArch 코어, Dolphin NX, Dekopon, Cemu-nx
- 최근 실행 30개, 즐겨찾기(Y)
- 게임을 끝내면 런처로 돌아오기 (설정에서 켜기, 원래 hbmenu는 백업 후 설정에서 열 수 있음)
- 앨범(메모리 부족) 모드에서 무거운 기종 실행 시 경고
- 상단에 폰 화면 주소(스위치 IP:8080) 표시
- 한글 표시 (스위치 내장 폰트 사용)

## 설정 파일
`sdmc:/config/nxlauncher/config.ini` (첫 실행 때 자동 생성)
- `rom_root` : 롬 최상위 폴더
- `[기종]` 마다 `folder`, `extensions`, `emulators`(`|`로 후보 여러 개), `args`(`{path}` 자리에 게임 경로)

## 조작
A 실행 · Y 즐겨찾기 · L/R(또는 좌우) 기종 · ZL/ZR 10칸 이동 · − 설정 · + 종료

## 문제가 생기면
"런처로 돌아오기"를 켠 뒤 런처가 안 뜨면, SD카드에서
`config/nxlauncher/hbmenu_original.nro` 를 `hbmenu.nro` 로 최상위에 복사하면 원래대로 돌아가요.

## 빌드
GitHub에 올리면 Actions가 자동 빌드 → Artifacts의 `nxlauncher.nro`.
설치 위치: `sdmc:/switch/nxlauncher/nxlauncher.nro`
