#include "pch.h"
#include "screw_processor.h"

#include <algorithm>
#include <cstring>
#include <cmath>
#include <array>

// 외부 라이브러리
#include <opencv2/dnn.hpp>
#include <onnxruntime_cxx_api.h>

static void DrawGrid(cv::Mat& img, int spacing = 100, int thickness = 1) // 격자 그리는 함수, spacing: 격자 간격, thickness: 선 두께
{
    if (img.empty() || spacing <= 0) return;

    for (int x = 0; x < img.cols; x += spacing)
    {
        cv::line(
            img,
            cv::Point(x, 0), // 시작점
            cv::Point(x, img.rows - 1), //끝점
            cv::Scalar(255, 255, 255), //색상
            thickness, // 1
            cv::LINE_4 //4방향 연결
        );
    }

    for (int y = 0; y < img.rows; y += spacing)
    {
        cv::line(
            img,
            cv::Point(0, y),
            cv::Point(img.cols - 1, y),
            cv::Scalar(255, 255, 255),
            thickness,
            cv::LINE_4
        );
    }
}

ScrewProcessor::ScrewProcessor() //생성자 
	: m_env(nullptr) // ONNX Runtime 환경 객체 초기화 (nullptr로 시작)
	, m_session_options() // ONNX Runtime 세션 옵션 객체 초기화 (기본값으로 시작)
	, m_session(nullptr) // ONNX Runtime 세션 객체 초기화 (nullptr로 시작)
{
    m_session_options.SetIntraOpNumThreads(1); // 한 연산 내부에서 사용할 CPU 스레드 수 (1개 제한)
    m_session_options.SetGraphOptimizationLevel(  // ONNX 모델을 자동으로 최적화해서 빠르게 실행
        GraphOptimizationLevel::ORT_ENABLE_EXTENDED); 
}

ScrewProcessor::~ScrewProcessor() = default; // 디폴트 소멸자

bool ScrewProcessor::loadModel(const std::wstring& model_path) //모델 로드
{
    try {
		if (!m_env) {  // 만약 비어있다면 
			m_env = std::make_unique<Ort::Env>( 
				ORT_LOGGING_LEVEL_WARNING, "screw_finder"); // ONNX Runtime 환경 객체 생성, 로그 레벨은 WARNING, 로그 ID는 "screw_finder"
        }
		m_session.reset(); // 기존 세션이 있다면 리셋
		m_session = std::make_unique<Ort::Session>( // ONNX Runtime 세션 객체 생성, 모델 로드
			*m_env, model_path.c_str(), m_session_options); //model_path.c_str(),  C 스타일 문자열로 변환, ONNX C 타입 요구
        model_loaded_ = true; // 모델 로드 성공 표시
        return true;
    }
    catch (const Ort::Exception&) { // ONNX Runtime 관련 예외
        model_loaded_ = false;
        return false;
    }
    catch (...) { // 모든 기타 예외
        model_loaded_ = false;
        return false;
    }
}

//------------------------------------------------------------------------------------------------------//

// ScrewProcessor::LetterboxInfo 반환
ScrewProcessor::LetterboxInfo ScrewProcessor::letterbox(const cv::Mat& src, int target_w, int target_h) 
{
    LetterboxInfo info;
	int src_w = src.cols; // 원본 이미지의 너비
	int src_h = src.rows; // 원본 이미지의 높이
    float r = std::min((float)target_w / src_w, (float)target_h / src_h); //리사이즈 비율 계산
	int new_w = (int)std::round(src_w * r); // 리사이즈된 이미지의 너비 계산
	int new_h = (int)std::round(src_h * r); // 리사이즈된 이미지의 높이 계산
    info.scale = r; // 비율 저장 
    info.pad_w = (target_w - new_w) / 2; // 좌우 균등
    info.pad_h = (target_h - new_h) / 2; // 상하 균등

	cv::Mat resized;  // 리사이즈 될 이미지 저장할 Mat 객체
    cv::resize(src, resized, cv::Size(new_w, new_h)); // 크기 조절

    cv::Mat canvas(target_h, target_w, CV_8UC3, cv::Scalar(114, 114, 114));  // 회색 캔버스 만들기
    resized.copyTo(canvas(cv::Rect(info.pad_w, info.pad_h, new_w, new_h))); // 회색 캔버스에 이미지 붙히기
	info.image = canvas; // 결과 이미지 저장
    return info;
}

// std::vector<float> 반환
std::vector<float> ScrewProcessor::blobFromImageCHW(const cv::Mat& image_bgr) 
{
    cv::Mat rgb;
	cv::cvtColor(image_bgr, rgb, cv::COLOR_BGR2RGB); // BGR 이미지를 RGB로 변환 ( ONNX 모델이 RGB 입력을 기대하기 때문)

    cv::Mat f32;  
	rgb.convertTo(f32, CV_32F, 1.0 / 255.0); // 0-255 범위를 0-1 범위로 정규화 => 활성화 함수 튀는  값 방지

	std::vector<cv::Mat> chw(3); // 채널별로 분리할 Mat 객체 벡터 (3채널)
	cv::split(f32, chw); //HWC => CHW 변환 위해 채널별로 분리

	std::vector<float> blob(3 * image_bgr.rows * image_bgr.cols); // 모델 입력에 맞는 1차원 벡터로 변환할 공간 할당, 크기는 채널 수 * 높이 * 너비
    int channel_size = image_bgr.rows * image_bgr.cols;

    for (int c = 0; c < 3; ++c)
		std::memcpy(blob.data() + c * channel_size, chw[c].data, channel_size * sizeof(float)); // 각 채널의 데이터를 blob 벡터에 채널별로 복사 (CHW 형식 유지)

    return blob;
}

//------------------------------------------------------------------------------------------------------//

float ScrewProcessor::sigmoid(float x) //활성화 함수
{
    return 1.0f / (1.0f + std::exp(-x));
}

float ScrewProcessor::clampf(float v, float lo, float hi) //클램프 함수
{
    return std::max(lo, std::min(v, hi));
}

cv::Rect ScrewProcessor::clampRect(const cv::Rect& r, int w, int h)
{
    int x = std::max(0, r.x), y = std::max(0, r.y);
    int x2 = std::min(w, r.x + r.width), y2 = std::min(h, r.y + r.height);
    return cv::Rect(x, y, std::max(0, x2 - x), std::max(0, y2 - y));
}

// ProcessResult 구조체 반환
ProcessResult ScrewProcessor::processImage(const std::string& image_path)
{
    ProcessResult result;
    if (!model_loaded_ || !m_session) {
        result.error_message = "모델이 로드되지 않았습니다.";
        return result;
    }

//----------------------------------------------------------------------------------------------------------//

    const float conf_thres = 0.30f;
    const float mask_thres = 0.8f;
    const float nms_iou_thres = 0.45f;

    try { 
        cv::Mat original = cv::imread(image_path); // 이미지 로드
        if (original.empty()) {
            result.error_message = "이미지 로드 실패";
            return result;

        }

		LetterboxInfo lb = letterbox(original, 640, 640); // 모델 입력 크기에 맞게 리사이즈 및 패딩 처리
		std::vector<float> input_tensor_values = blobFromImageCHW(lb.image); // 이미지 전처리: BGR -> RGB, 0-255 -> 0-1, HWC -> CHW
		std::array<int64_t, 4> input_shape{ 1, 3, 640, 640 }; // 모델 입력 텐서의 형태 ( 입력 사진 1장, 3채널, 640x640 크기)

        Ort::AllocatorWithDefaultOptions allocator; //이 구조체는 ONNX Runtime이 기본적으로 소유하고(owned by) 있는 '기본 메모리 할당자(OrtAllocator)'를 C++에서 쓰기 편하게 포장(Wrapper)해 놓은 것
        auto input_name_alloc = m_session->GetInputNameAllocated(0, allocator); // 0번 입력 구멍
        auto output_name0_alloc = m_session->GetOutputNameAllocated(0, allocator); // 0번 출력 구멍: 나사의 네모 박스(Bounding Box) 위치와 확률 정보
		auto output_name1_alloc = m_session->GetOutputNameAllocated(1, allocator); // 1번 출력 구멍: 나사의 실제 윤곽선을 색칠하기 위한 마스크(Mask) 정보

		const char* input_name = input_name_alloc.get(); // 모델의 입력 텐서 이름
		const char* output_names[] = { output_name0_alloc.get(), output_name1_alloc.get() }; // 모델의 출력 텐서 이름 배열

        // 메모리의 출처와 특성을 명시하는 역할을 하는 구조체
		Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault); // CPU 메모리 정보 생성
       
        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
            memory_info,
            input_tensor_values.data(),
            input_tensor_values.size(),
            input_shape.data(),
            input_shape.size());

		auto output_tensors = m_session->Run( // 모델 실행
			Ort::RunOptions{ nullptr }, // 실행 옵션 (기본값)
            &input_name, //입력 구멍 이름
            &input_tensor, //이미지 데이터
			1, //입력 텐서 개수
			output_names, //출력 구멍 이름 배열
			2); //출력 텐서 개수

		float* output0 = output_tensors[0].GetTensorMutableData<float>(); // 모델의 첫 번째 출력 텐서 데이터 포인터 (바운딩 박스 좌표와 확률 정보가 들어있는 상자)
		float* output1 = output_tensors[1].GetTensorMutableData<float>(); // 모델의 두 번째 출력 텐서 데이터 포인터 ( 실제 픽셀(마스크) 데이터가 시작되는 메모리의 첫 번째 주소)

		auto out0_shape = output_tensors[0].GetTensorTypeAndShapeInfo().GetShape(); // 첫 번째 출력 텐서의 형태 정보에서 형태 벡터 가져오기
		auto out1_shape = output_tensors[1].GetTensorTypeAndShapeInfo().GetShape(); // 두 번째 출력 텐서의 형태 정보에서 형태 벡터 가져오기

        // [0번 상자: 바운딩 박스 정보]
        int num_det = (int)out0_shape[1];  // AI가 사진 속에서 "물체가 있을 것 같다"고 예측한 '네모 박스(후보군)의 총 개수' 
        int num_attr = (int)out0_shape[2]; // 박스 1개당 들어있는 '정보(속성)의 개수' (좌표 4개 + 확률 + 클래스 ID + 마스크 그릴 때 쓸 물감 번호들)

        // [1번 상자: 픽셀 마스크 정보] 
        int num_proto = (int)out1_shape[1]; // 픽셀을 색칠하기 위해 AI가 준비한 '기본 도화지(프로토타입)의 개수'
        int proto_h = (int)out1_shape[2];   // 그 도화지의 '세로 높이' 
        int proto_w = (int)out1_shape[3];   // 그 도화지의 '가로 너비' 

        //------------------------------------------------------------------------------------------------------//

		std::vector<Detection> detections; // 감지된 객체 정보를 저장할 벡터

        for (int i = 0; i < num_det; ++i) {
            const float* row = output0 + i * num_attr;
            float conf = row[4]; //i번째 후보의 신뢰도(confidence) 값을 꺼냄
			if (conf < conf_thres) continue; // 신뢰도가 임계값보다 낮으면 무시하고 다음 후보로 넘어감

            Detection det;
			det.x1 = row[0]; // i번째 후보의 바운딩 박스 좌표 정보 
			det.y1 = row[1]; // i번째 후보의 바운딩 박스 좌표 정보
			det.x2 = row[2]; // i번째 후보의 바운딩 박스 좌표 정보
			det.y2 = row[3]; // i번째 후보의 바운딩 박스 좌표 정보
			det.conf = conf; // i번째 후보의 신뢰도(confidence) 값을 저장
			det.class_id = (int)row[5]; // i번째 후보의 클래스 ID (예: 나사 종류) 저장
			det.mask_coeffs.assign(row + 6, row + 38); // i번째 후보의 마스크 그릴 때 사용할 물감 번호들(마스크 계수) 저장
            detections.push_back(det);
        }

		std::vector<cv::Rect> nms_boxes; // NMS 처리를 위한 바운딩 박스 벡터
		std::vector<float> nms_scores; // NMS 처리를 위한 신뢰도 점수 벡터

        for (const auto& d : detections) { //모델이 640×640 속도 조정된 이미지 안에서 예측한 바운딩 박스 좌표들을, 다시 원본 이미지 좌표로 정확히 돌려놓는 역변환
            float x1 = clampf((d.x1 - lb.pad_w) / lb.scale, 0.0f, (float)(original.cols - 1));
            float y1 = clampf((d.y1 - lb.pad_h) / lb.scale, 0.0f, (float)(original.rows - 1));
            float x2 = clampf((d.x2 - lb.pad_w) / lb.scale, 0.0f, (float)(original.cols - 1));
            float y2 = clampf((d.y2 - lb.pad_h) / lb.scale, 0.0f, (float)(original.rows - 1));

            nms_boxes.push_back(cv::Rect(
                (int)x1,
                (int)y1, // x, y는 왼쪽·위쪽 좌표(왼쪽 상단 모서리)
                std::max(1, (int)(x2 - x1)), 
				std::max(1, (int)(y2 - y1)) // width, height는 박스의 크기(너비와 높이)
            ));
            nms_scores.push_back(d.conf); // 신뢰도 값 저장
        }

		std::vector<int> indices; // detections 벡터의 몇 번째 Detection이 살아남았는지를 저장

		cv::dnn::NMSBoxes(nms_boxes, nms_scores, conf_thres, nms_iou_thres, indices); // NMS 처리: 겹치는 박스 제거

		std::vector<Detection> filtered; // NMS 처리 후 남은 감지된 객체 정보를 저장할 벡터
        
        for (int idx : indices) //NMS가 살아남게 한 Detection들만 골라서 새로운 filtered 벡터에 다시 모아 넣기
            filtered.push_back(detections[idx]);

        detections = filtered; // filtered 벡터를, 앞으로 계속 쓸 detections에 덮어쓰기

		cv::Mat vis = original.clone(); // 시각화를 위한 원본 이미지 복제
        int screw_id = 1; 

		//--------------------------------------------------------------------------------------//

        for (const auto& d : detections) {
            cv::Mat mask_logits(proto_h, proto_w, CV_32F, cv::Scalar(0));

            for (int c = 0; c < num_proto; ++c) {
                float coeff = d.mask_coeffs[c];
                const float* proto_ptr = output1 + c * proto_h * proto_w;

                for (int y = 0; y < proto_h; ++y) {
                    float* row_ptr = mask_logits.ptr<float>(y);
                    const float* p = proto_ptr + y * proto_w;
                    for (int x = 0; x < proto_w; ++x)
                        row_ptr[x] += coeff * p[x];
                }
            }

            for (int y = 0; y < proto_h; ++y) {
                float* row_ptr = mask_logits.ptr<float>(y);
                for (int x = 0; x < proto_w; ++x)
                    row_ptr[x] = sigmoid(row_ptr[x]);
            }

            float x1 = clampf((d.x1 - lb.pad_w) / lb.scale, 0.0f, (float)(original.cols - 1));
            float y1 = clampf((d.y1 - lb.pad_h) / lb.scale, 0.0f, (float)(original.rows - 1));
            float x2 = clampf((d.x2 - lb.pad_w) / lb.scale, 0.0f, (float)(original.cols - 1));
            float y2 = clampf((d.y2 - lb.pad_h) / lb.scale, 0.0f, (float)(original.rows - 1));

			// ------------------------------------------------------------------------------//

            //방금 만든 마스크 좌표를 기준으로, 원본 이미지 위에 그릴 바운딩 박스(cv::Rect) 하나를 정의하는 과정
			cv::Rect box( 
                (int)x1,
                (int)y1,
                std::max(1, (int)(x2 - x1)),
                std::max(1, (int)(y2 - y1))
            );

            box = clampRect(box, original.cols, original.rows);
            if (box.width <= 0 || box.height <= 0) continue;

            float sx1 = d.x1 / 640.0f * proto_w; //640×640 모델 입력 좌표를, 프로토타입(마스크 도화지)의 해상도로 다시 비율 맞춰서 변환하는 것
            float sy1 = d.y1 / 640.0f * proto_h;
            float sx2 = d.x2 / 640.0f * proto_w;
            float sy2 = d.y2 / 640.0f * proto_h;

            // 프로토타입(마스크 도화지) 위에서, 현재 나사가 차지하는 영역을 정수 좌표·크기로 정의
            cv::Rect proto_box( 
                (int)std::floor(sx1),
                (int)std::floor(sy1),
                std::max(1, (int)std::ceil(sx2 - sx1)),
                std::max(1, (int)std::ceil(sy2 - sy1))
            );

            proto_box = clampRect(proto_box, proto_w, proto_h);
            if (proto_box.width <= 0 || proto_box.height <= 0) continue;

			cv::Mat mask_crop = mask_logits(proto_box).clone();  // 프로토타입 mask_logits에서 proto_box 영역만 잘라냄

			cv::Mat mask_resized; 
			cv::resize(mask_crop, mask_resized, box.size(), 0, 0, cv::INTER_LINEAR); // 프로토타입 기준으로 잘라 둔 mask_crop을, 원래 나사 바운딩 박스 box의 크기에 맞게 다시 키우는(리사이즈) 과정

			cv::Mat binary_mask; 
            cv::threshold(mask_resized, binary_mask, mask_thres, 255, cv::THRESH_BINARY); // 임계값보다 큰 픽셀은 255로, 같거나 작은 픽셀은 0으로 바꾸는 이진화
			binary_mask.convertTo(binary_mask, CV_8U); // 이진화 타입을 8비트 정수로 고정

			int mask_area = cv::countNonZero(binary_mask); // 이 나사가 차지하는 픽셀 수(=나사의 세그멘테이션 면적)

			cv::Mat roi = vis(box);  // vis: 전체 시각화 이미지 , box: cv::Rect로, 나사가 차지하는 영역입니다. / cv::Mat () 오퍼레이터 사용
			cv::Mat overlay = roi.clone(); 
			overlay.setTo(cv::Scalar(120, 255, 120), binary_mask); // 나사가 있는 부분만 연두색으로 덮어씌움
            cv::addWeighted(overlay, 0.45, roi, 0.55, 0.0, roi);  //연두색 오버레이를 원래 이미지에 45% 정도 섞어서 투명하게 겹쳐 보이게 함

            std::vector<std::vector<cv::Point>> contours; // 윤곽선  벡터
            cv::findContours(binary_mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE); // contours는 나사 외곽선의 점들 좌표 리스트
            for (auto& contour : contours) // 좌표 옮기기
                for (auto& pt : contour) {
                    pt.x += box.x;
                    pt.y += box.y;
                }

            cv::drawContours(vis, contours, -1, cv::Scalar(0, 120, 0), 4); // 윤곽선 그리기

            //나사의 중심 좌표
            int cx = box.x + box.width / 2;
            int cy = box.y + box.height / 2;
            cv::Moments mu = cv::moments(binary_mask, true);
            if (mu.m00 > 1e-5) {
                cx = box.x + (int)(mu.m10 / mu.m00);
                cy = box.y + (int)(mu.m01 / mu.m00);
            }

			// 나사 정보 저장
            ScrewResult r;
            r.screw_id = screw_id;
            r.class_id = d.class_id;
            r.confidence = d.conf;
            r.center_x = cx;
            r.center_y = cy;
            r.bbox_x = box.x;
            r.bbox_y = box.y;
            r.bbox_w = box.width;
            r.bbox_h = box.height;
            r.mask_area = mask_area;
            r.contours = contours;
            r.binary_mask = binary_mask.clone();

            result.screws.push_back(r);
            screw_id++;
        }

		// 격자 그리기 (선택 사항)
        DrawGrid(vis, 100, 1);

		result.original_image = original.clone(); // 원본 이미지 저장
		result.overlay_image = vis; // 시각화 이미지 저장
		result.success = true; // 처리 성공 표시
        return result;
    }
	catch (const Ort::Exception& e) { // ONNX Runtime 관련 예외 처리
        result.error_message = e.what();
        return result;
    }
	catch (const std::exception& e) { // 모든 표준 예외 처리
        result.error_message = e.what();
        return result;
    }
}