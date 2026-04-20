#pragma once

#include <string>
#include <vector>
#include <memory>
#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>

struct Detection // 모델이 예측한 나사 위치와 클래스 정보를 담는 구조체
{
    float x1 = 0.0f;
    float y1 = 0.0f;
    float x2 = 0.0f;
    float y2 = 0.0f;
    float conf = 0.0f;
    int class_id = 0;
    std::vector<float> mask_coeffs;
};

struct ScrewResult
{
    int screw_id = 0;
    int class_id = 0;
    float confidence = 0.0f;
    int center_x = 0;
    int center_y = 0;
    int bbox_x = 0;
    int bbox_y = 0;
    int bbox_w = 0;
    int bbox_h = 0;
    int mask_area = 0;

    std::vector<std::vector<cv::Point>> contours;
    cv::Mat binary_mask;
};

struct ProcessResult // 이미지 처리 결과를 담는 구조체
{
    bool success = false;
    std::string error_message;
    std::vector<ScrewResult> screws;
    cv::Mat original_image;
    cv::Mat overlay_image;
};

class ScrewProcessor
{
public:
    ScrewProcessor(); // 생성자 
    ~ScrewProcessor(); // 소멸자(디폴트)

	bool loadModel(const std::wstring& model_path); // ONNX 모델을 로드하는 함수
    ProcessResult processImage(const std::string& image_path);

private:
	struct LetterboxInfo // 모델 입력 크기에 맞게 이미지를 리사이즈하고 패딩 처리한 결과를 담는 구조체
    {
        cv::Mat image;
        float scale = 1.0f; 
        int pad_w = 0;
        int pad_h = 0;
    };

	LetterboxInfo letterbox(const cv::Mat& src, int target_w, int target_h); // 모델 입력 크기에 맞게 이미지를 리사이즈하고 패딩 처리 함수
	std::vector<float> blobFromImageCHW(const cv::Mat& image_bgr); // 이미지를 모델 입력에 맞는 1차원 벡터로 변환하는 함수 (CHW 형식)
	float sigmoid(float x); // 시그모이드 함수 (활성화 함수로 사용)
	float clampf(float v, float lo, float hi); // 클램프 함수 
	cv::Rect clampRect(const cv::Rect& r, int w, int h); // 주어진 사각형이 이미지 경계를 벗어나지 않도록 클램프하는 함수

private:
    bool model_loaded_ = false;
    std::unique_ptr<Ort::Env> m_env; // ONNX Runtime의 글로벌 설정값과 제어 포인터
    Ort::SessionOptions m_session_options; // AI 모델을 어떤 방식으로, 얼마나 빠르고 효율적으로, 어떤 하드웨어를 써서 돌릴 것인지 "실행 설계서"를 작성하는 도구
    std::unique_ptr<Ort::Session> m_session; // m_session = std::make_unique<Ort::Session>(*m_env, "my_model.onnx", m_session_options);
};