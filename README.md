# 🔩 Screw Finder (비전 나사 검출기)

### 비전 기반 나사 검출 및 위치 분석 시스템

<p align="center">
  <img src="https://github.com/user-attachments/assets/ff39b7da-bcd8-4552-b770-9c429af549c2" width="650"/>
</p>

> **입력 이미지에서 나사를 검출하고, 위치·신뢰도·세그멘테이션 결과를 시각화하는 Windows 기반 비전 검사 도구**

---

## 1. 핵심 정보 요약

| 항목 | 내용 |
|------|------|
| **프로젝트명** | Screw Finder |
| **개발 기간** | 2025.03 ~ 2025.04 |
| **프로젝트 유형** | 컴퓨터 비전 / Windows 애플리케이션 / 검사 도구 |
| **개발 인원** | 1명 |
| **역할** | 비전 처리 로직 구현, ONNX 모델 연동, MFC UI 개발 |
| **기술 스택** | C++, OpenCV, ONNX Runtime, MFC |
| **핵심 성과** | 나사 검출 결과를 시각화하고, 좌표 및 신뢰도 정보를 CSV·이미지·ZIP 형태로 저장할 수 있는 검사 프로그램 구현 |

---

## 2. 주요 기능

### 1) 이미지 기반 나사 검출
- ONNX 모델을 로드하여 입력 이미지에서 나사를 검출
- 검출 후보의 위치, 신뢰도, 마스크 정보를 기반으로 후처리 수행

### 2) 세그멘테이션 결과 시각화
- 검출된 나사 영역에 색상 오버레이와 윤곽선을 함께 표시
- 결과 이미지에 격자를 추가하여 위치를 직관적으로 확인할 수 있도록 구성

### 3) 리스트 기반 결과 확인
- 검출된 각 나사를 썸네일 이미지와 함께 리스트에 표시
- ID, 중심 좌표(X/Y), 신뢰도를 UI에서 바로 확인 가능

### 4) 선택 객체 상세 확인
- 리스트에서 특정 나사를 선택하면 해당 객체만 강조 표시
- 선택된 나사의 마스크와 윤곽선을 다시 시각화하여 개별 결과 확인 가능

### 5) 결과 저장 기능
- 전체 결과 이미지를 저장
- 검출 좌표 및 신뢰도를 CSV 파일로 저장
- 각 나사 이미지를 개별 PNG로 저장
- 최종적으로 ZIP 파일로 묶어 내보내기 지원

---

## 3. 시스템 아키텍처

### Vision Engine (C++)
- ONNX Runtime 기반 모델 로드 및 추론 수행
- OpenCV를 이용한 전처리, NMS, 마스크 생성, 후처리 구현

### Desktop UI (MFC)
- 이미지 선택, 추론 실행, 결과 리스트 표시 기능 제공
- 선택 객체 강조 및 결과 확인을 위한 인터랙션 구성

### Export Module
- 결과 이미지 저장
- CSV 저장
- 개별 나사 이미지 저장
- ZIP 압축 및 내보내기

---

## 4. 구현 내용

<p align="center">
  <img src="https://github.com/user-attachments/assets/a509bdc4-4601-46c8-9c76-443bfcc7dc01" width="535"/>
</p>

### 1) ONNX 기반 나사 검출 파이프라인 구현
- ONNX Runtime을 활용해 학습된 모델을 로드하고 추론 수행
- 입력 이미지를 모델 형식에 맞게 전처리한 뒤 검출 결과 생성
- 검출 출력과 마스크 출력을 기반으로 객체별 후처리 로직 구현

### 2) 마스크 기반 후처리 및 시각화 구현
- 검출 후보에 대해 NMS를 적용하여 중복 객체 제거
- 객체별 마스크를 복원한 뒤 이진화하여 세그멘테이션 결과 생성
- 윤곽선, 중심 좌표, 면적 정보를 계산하고 오버레이 이미지에 반영

### 3) 결과 데이터 구조화 및 활용
- 객체별 ID, 중심 좌표, 신뢰도, 마스크 정보를 구조체 형태로 관리
- 검출 결과를 리스트, 시각화 이미지, 저장 파일로 연계할 수 있도록 구성
- 후속 저장 및 UI 표시 로직과 자연스럽게 연결되도록 처리

### 4) MFC 기반 결과 표시 UI
- 이미지 열기 / 실행 / 저장 버튼 기반 워크플로우 구성
- 리스트 컨트롤에 썸네일, ID, X축, Y축, 신뢰도 표시
- 항목 선택 시 해당 나사만 다시 강조해 보여주는 인터랙션 구현

### 5) 결과 저장 기능 구현
- 검출 결과를 `ID, X, Y, Confidence` 형식의 CSV로 저장
- 각 검출 객체를 개별 PNG로 저장
- 전체 결과 이미지와 함께 ZIP 파일로 압축하여 출력

---

## 5. 실제 결과물 / 시연 이미지 / 저장 결과

### 결과 화면
<p align="center">
  <img src="https://github.com/user-attachments/assets/ff39b7da-bcd8-4552-b770-9c429af549c2" width="650"/>
</p>

### 유튜브 영상
<p align="center">
  <a href="https://youtu.be/ZSSlrIAlYAU">
    <img src="https://img.youtube.com/vi/ZSSlrIAlYAU/maxresdefault.jpg" width="700"/>
  </a>
</p>

### 저장 결과 예시

#### `result.png`
- 전체 시각화 결과 이미지

#### `result.csv`
- 검출 좌표 및 신뢰도 정보

<p>
  <img src="https://github.com/user-attachments/assets/de8ece00-f5a8-434c-b481-ff55ab811d39" width="420"/>
</p>

#### `heads/`
- 개별 나사 이미지

<table >
  <tr>
    <td><img src="https://github.com/user-attachments/assets/fb2d4fb4-9c97-4875-93ac-fa74d8588776" width="100" height="100"/></td>
    <td><img src="https://github.com/user-attachments/assets/b2423491-3617-4599-8d19-c54103a735bb" width="100" height="100"/></td>
  </tr>
</table>

#### `result.zip`
- 전체 결과 압축 파일

---

## 6. 기술 스택

- **Language**: C++
- **Computer Vision**: OpenCV
- **Inference**: ONNX Runtime
- **Desktop UI**: MFC
- **Output**: CSV, PNG, ZIP

---

## 7. 프로젝트 요약

Screw Finder는 입력 이미지에서 나사를 자동으로 검출하고,  
각 객체의 **위치, 신뢰도, 세그멘테이션 결과**를 시각적으로 확인할 수 있도록 구현한 Windows 기반 비전 애플리케이션입니다.

단순한 객체 검출에 그치지 않고,  
ONNX 기반 추론, 마스크 후처리, MFC UI 구성, 결과 저장 기능까지 직접 구현하여  
실제 검사 보조 도구 형태로 활용할 수 있도록 완성했습니다.
