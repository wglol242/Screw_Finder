#include "pch.h"
#include "framework.h"
#include "MFC_SCREW.h"
#include "MFC_SCREWDlg.h"
#include "afxdialogex.h"

#include <atlconv.h>
#include <opencv2/opencv.hpp>
#include <fstream>
#include <vector>
#include <algorithm>
#include <windows.h>
#include <shellapi.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

static void DrawWhiteGrid(cv::Mat& img, int spacing = 100, int thickness = 1)
{
    if (img.empty() || spacing <= 0) return;

    for (int x = 0; x < img.cols; x += spacing)
    {
        cv::line(
            img,
            cv::Point(x, 0),
            cv::Point(x, img.rows - 1),
            cv::Scalar(255, 255, 255),
            thickness,
            cv::LINE_8
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
            cv::LINE_8
        );
    }
}

CMFCSCREWDlg::CMFCSCREWDlg(CWnd* pParent /*=nullptr*/) // 생성자: 부모 창 X 
    : CDialogEx(IDD_SCREWFINDERMFC_DIALOG, pParent) //다이알로그 ID 연결
{}

void CMFCSCREWDlg::DoDataExchange(CDataExchange* pDX)  //DoDataExchange = 데이터 교환/연결 작업 하는 함수
{
	CDialogEx::DoDataExchange(pDX); // 부모클래스 DoDataExchange 호출
	DDX_Control(pDX, IDC_PIC_RESULT, m_picResult); // IDC_PIC_RESULT 컨트롤과 m_picResult 멤버 변수 연결
	DDX_Control(pDX, IDC_LIST_RESULT, m_listResult); // IDC_LIST_RESULT 컨트롤과 m_listResult 멤버 변수 연결
}

CString CMFCSCREWDlg::ReplaceExtension(const CString& path, const CString& newExt)
{
    CString result = path;
	int dotPos = result.ReverseFind(_T('.')); // 확장자 시작 위치 찾기
    if (dotPos >= 0) 
        result = result.Left(dotPos);

	result += newExt; // 새로운 확장자 추가
    return result;
}

bool CMFCSCREWDlg::SaveResultCsv(const CString& csvPath) // 결과를 CSV 파일로 저장하는 함수
{
    CT2A pathA(csvPath, CP_UTF8);
    std::ofstream ofs(pathA);
    if (!ofs.is_open())
        return false;

    ofs << "\xEF\xBB\xBF";
    ofs << "ID,X,Y,Confidence\n";

    for (const auto& s : m_lastResult.screws)
    {
        ofs << s.screw_id << ","
            << s.center_x << ","
            << s.center_y << ","
            << s.confidence
            << "\n";
    }

    return true;
}

bool CMFCSCREWDlg::SaveHeadImages(const CString& headsFolderPath) // 나사 머리 이미지를 저장하는 함수
{
    if (m_lastResult.original_image.empty())
        return false;

    if (!CreateDirectory(headsFolderPath, NULL))
    {
        DWORD err = GetLastError();
        if (err != ERROR_ALREADY_EXISTS)
            return false;
    }

    for (const auto& s : m_lastResult.screws)
    {
        cv::Rect cropRect(s.bbox_x, s.bbox_y, s.bbox_w, s.bbox_h);

        cropRect.x = std::max(0, cropRect.x);
        cropRect.y = std::max(0, cropRect.y);

        if (cropRect.x + cropRect.width > m_lastResult.original_image.cols)
            cropRect.width = m_lastResult.original_image.cols - cropRect.x;

        if (cropRect.y + cropRect.height > m_lastResult.original_image.rows)
            cropRect.height = m_lastResult.original_image.rows - cropRect.y;

        if (cropRect.width <= 0 || cropRect.height <= 0)
            continue;

        cv::Mat crop = m_lastResult.original_image(cropRect).clone();

        CString headPath;
        headPath.Format(_T("%s\\%d.png"), headsFolderPath, s.screw_id);

        CT2A headPathA(headPath, CP_UTF8);
        if (!cv::imwrite(std::string(headPathA), crop))
            return false;
    }

    return true;
}

bool CMFCSCREWDlg::CreateZipFromFolder(const CString& sourceFolderPath, const CString& zipPath) // 폴더를 ZIP 파일로 압축하는 함수
{
    CString command;
    command.Format(
        _T("powershell -NoProfile -ExecutionPolicy Bypass -Command \"Compress-Archive -Path '%s\\*' -DestinationPath '%s' -Force\""),
        sourceFolderPath, zipPath);

    STARTUPINFO si = {};
    PROCESS_INFORMATION pi = {};
    si.cb = sizeof(si);

    std::vector<TCHAR> cmdLine(command.GetLength() + 1);
    _tcscpy_s(cmdLine.data(), cmdLine.size(), command);

    BOOL ok = CreateProcess(
        NULL,
        cmdLine.data(),
        NULL,
        NULL,
        FALSE,
        CREATE_NO_WINDOW,
        NULL,
        NULL,
        &si,
        &pi);

    if (!ok)
        return false;

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    DWORD attr = GetFileAttributes(zipPath);
    return (exitCode == 0 && attr != INVALID_FILE_ATTRIBUTES);
}

bool CMFCSCREWDlg::DeleteFolderRecursive(const CString& folderPath) //임시폴더 삭제 함수
{
    CString from = folderPath;
    from += _T('\0');

    SHFILEOPSTRUCT fos = {};
    fos.wFunc = FO_DELETE;
    fos.pFrom = from;
    fos.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;

    return SHFileOperation(&fos) == 0;
}

void CMFCSCREWDlg::AdjustListColumns() // 리스트 컨트롤의 열 너비를 조정하는 함수
{
    if (!::IsWindow(m_listResult.GetSafeHwnd()))
        return;

    CRect rc;
    m_listResult.GetClientRect(&rc);

    int totalWidth = rc.Width();
    totalWidth -= 2;

    int fixedWidth = 0;
    fixedWidth += m_listResult.GetColumnWidth(0);
    fixedWidth += m_listResult.GetColumnWidth(1);
    fixedWidth += m_listResult.GetColumnWidth(2);
    fixedWidth += m_listResult.GetColumnWidth(3);
    fixedWidth += m_listResult.GetColumnWidth(4);

    int lastWidth = totalWidth - fixedWidth;

    if (lastWidth < 80)
        lastWidth = 80;

    m_listResult.SetColumnWidth(5, lastWidth);
}

void CMFCSCREWDlg::ShowImageInControl(CStatic& ctrl, const cv::Mat& img) // OpenCV 이미지를 CStatic 컨트롤에 표시하는 함수
{
    if (img.empty()) return;

    CRect rect;
    ctrl.GetClientRect(&rect);
    ctrl.ClientToScreen(&rect);
    ScreenToClient(&rect);
    if (rect.Width() <= 0 || rect.Height() <= 0) return;

    cv::Mat resized;
    cv::resize(img, resized, cv::Size(rect.Width(), rect.Height()));

    int w = resized.cols;
    int h = resized.rows;
    int stride = ((w * 3 + 3) / 4) * 4;
    std::vector<uchar> buf(stride * h, 0);

    for (int y = 0; y < h; ++y) {
        const uchar* src_row = resized.ptr<uchar>(y);
        uchar* dst_row = buf.data() + y * stride;
        std::memcpy(dst_row, src_row, w * 3);
    }

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biSizeImage = stride * h;

    CDC* pDC = GetDC();
    if (!pDC) return;

    SetStretchBltMode(pDC->GetSafeHdc(), COLORONCOLOR);
    StretchDIBits(pDC->GetSafeHdc(),
        rect.left, rect.top, rect.Width(), rect.Height(),
        0, 0, w, h,
        buf.data(), &bmi, DIB_RGB_COLORS, SRCCOPY);

    ReleaseDC(pDC);
}

void CMFCSCREWDlg::UpdateSelectedScrewDisplay() // 선택된 나사에 대한 디스플레이 업데이트 함수
{
    if (m_lastResult.original_image.empty())
        return;

    if (m_selectedScrewIndex < 0 ||
        m_selectedScrewIndex >= (int)m_lastResult.screws.size())
    {
        m_displayImage = m_lastResult.overlay_image.clone();
        ShowImageInControl(m_picResult, m_displayImage);
        return;
    }

    const auto& sel = m_lastResult.screws[m_selectedScrewIndex];
    cv::Mat display = m_lastResult.original_image.clone();

    cv::Rect box(sel.bbox_x, sel.bbox_y, sel.bbox_w, sel.bbox_h);
    if (box.x >= 0 && box.y >= 0 &&
        box.x + box.width <= display.cols &&
        box.y + box.height <= display.rows &&
        !sel.binary_mask.empty())
    {
        cv::Mat roi = display(box);
        cv::Mat overlay = roi.clone();
        overlay.setTo(cv::Scalar(120, 255, 120), sel.binary_mask);
        cv::addWeighted(overlay, 0.45, roi, 0.55, 0.0, roi);
    }

    if (!sel.contours.empty())
        cv::drawContours(display, sel.contours, -1, cv::Scalar(0, 120, 0), 4);

    DrawWhiteGrid(display, 100, 1);

    m_displayImage = display;
    ShowImageInControl(m_picResult, m_displayImage);
}

BEGIN_MESSAGE_MAP(CMFCSCREWDlg, CDialogEx) // 메시지 맵: 버튼 클릭, 리스트 아이템 변경 등 이벤트와 핸들러 함수 연결
    ON_BN_CLICKED(IDC_BTN_OPEN, &CMFCSCREWDlg::OnBnClickedBtnOpen)
    ON_BN_CLICKED(IDC_BTN_RUN, &CMFCSCREWDlg::OnBnClickedBtnRun)
    ON_BN_CLICKED(IDC_BTN_SAVE, &CMFCSCREWDlg::OnBnClickedBtnSave)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST_RESULT, &CMFCSCREWDlg::OnLvnItemchangedListResult)
END_MESSAGE_MAP()

BOOL CMFCSCREWDlg::OnInitDialog() // 다이알로그 초기화 함수
{
    CDialogEx::OnInitDialog();

    if (!AfxOleInit())
    {
        AfxMessageBox(_T("OLE 초기화 실패"));
        return FALSE;
    }

    SetWindowText(_T("나사 검출기 v1.0"));
    m_processor = std::make_unique<ScrewProcessor>();

    m_imageList.Create(60, 60, ILC_COLOR24, 10, 10);
    m_listResult.SetImageList(&m_imageList, LVSIL_SMALL);

    m_listResult.SetExtendedStyle(
        m_listResult.GetExtendedStyle() |
        LVS_EX_SUBITEMIMAGES |
        LVS_EX_FULLROWSELECT);

    m_listResult.InsertColumn(0, _T(""), LVCFMT_CENTER, 0);
    m_listResult.InsertColumn(1, _T("이미지"), LVCFMT_LEFT, 60);
    m_listResult.InsertColumn(2, _T("ID"), LVCFMT_CENTER, 40);
    m_listResult.InsertColumn(3, _T("X 축"), LVCFMT_CENTER, 80);
    m_listResult.InsertColumn(4, _T("Y 축"), LVCFMT_CENTER, 80);
    m_listResult.InsertColumn(5, _T("신뢰도"), LVCFMT_CENTER, 80);

    AdjustListColumns();

    if (!m_processor->loadModel(L"best.onnx"))
        AfxMessageBox(_T("모델 로드 실패"));

    HICON hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
    SetIcon(hIcon, TRUE);
    SetIcon(hIcon, FALSE);

    return TRUE;
}

void CMFCSCREWDlg::OnBnClickedBtnOpen() // "열기" 버튼 클릭 핸들러 함수
{
    CFileDialog dlg(
        TRUE, _T("jpg"), NULL, OFN_FILEMUSTEXIST,
        _T("Image Files (*.jpg;*.jpeg;*.png;*.bmp)|*.jpg;*.jpeg;*.png;*.bmp||")
    );

    if (dlg.DoModal() == IDOK)
    {
        m_currentImagePath = dlg.GetPathName();
        AfxMessageBox(_T("이미지 선택 완료"));
    }
}

void CMFCSCREWDlg::OnBnClickedBtnRun() // "실행" 버튼 클릭 핸들러 함수
{
    if (m_currentImagePath.IsEmpty())
    {
        AfxMessageBox(_T("먼저 이미지를 선택하세요."));
        return;
    }

    CT2A pathA(m_currentImagePath);
    m_lastResult = m_processor->processImage(std::string(pathA));

    if (!m_lastResult.success)
    {
        CString msg(m_lastResult.error_message.c_str());
        AfxMessageBox(_T("처리 실패: ") + msg);
        return;
    }

    m_listResult.DeleteAllItems();
    m_imageList.DeleteImageList();
    m_imageList.Create(60, 60, ILC_COLOR24, 10, 10);
    m_listResult.SetImageList(&m_imageList, LVSIL_SMALL);

    for (int i = 0; i < (int)m_lastResult.screws.size(); i++)
    {
        const auto& s = m_lastResult.screws[i];

        cv::Rect crop_rect(s.bbox_x, s.bbox_y, s.bbox_w, s.bbox_h);
        cv::Mat crop = m_lastResult.original_image(crop_rect).clone();
        cv::Mat thumb;
        cv::resize(crop, thumb, cv::Size(60, 60));

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = 60;
        bmi.bmiHeader.biHeight = -60;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 24;
        bmi.bmiHeader.biCompression = BI_RGB;

        int stride = ((60 * 3 + 3) / 4) * 4;
        std::vector<uchar> buf(stride * 60, 0);
        for (int y = 0; y < 60; ++y)
            std::memcpy(buf.data() + y * stride, thumb.ptr<uchar>(y), 60 * 3);

        HDC hdc = ::GetDC(NULL);
        HBITMAP hBmp = CreateDIBitmap(hdc, &bmi.bmiHeader,
            CBM_INIT, buf.data(), &bmi, DIB_RGB_COLORS);
        ::ReleaseDC(NULL, hdc);

        CBitmap bmp;
        bmp.Attach(hBmp);
        int imgIdx = m_imageList.Add(&bmp, (CBitmap*)NULL);
        bmp.Detach();
        DeleteObject(hBmp);

        LVITEM lvi0 = {};
        lvi0.mask = LVIF_TEXT;
        lvi0.iItem = i;
        lvi0.iSubItem = 0;
        lvi0.pszText = const_cast<LPTSTR>(_T(""));
        m_listResult.InsertItem(&lvi0);

        LVITEM lvi1 = {};
        lvi1.mask = LVIF_IMAGE;
        lvi1.iItem = i;
        lvi1.iSubItem = 1;
        lvi1.iImage = imgIdx;
        m_listResult.SetItem(&lvi1);

        CString str;
        str.Format(_T("%d"), s.screw_id);
        m_listResult.SetItemText(i, 2, str);

        str.Format(_T("%d"), s.center_x);
        m_listResult.SetItemText(i, 3, str);

        str.Format(_T("%d"), s.center_y);
        m_listResult.SetItemText(i, 4, str);

        str.Format(_T("%.3f"), s.confidence);
        m_listResult.SetItemText(i, 5, str);
    }

    AdjustListColumns();

    m_selectedScrewIndex = -1;
    m_displayImage = m_lastResult.overlay_image.clone();
    ShowImageInControl(m_picResult, m_displayImage);
}

void CMFCSCREWDlg::OnLvnItemchangedListResult(NMHDR* pNMHDR, LRESULT* pResult) // 리스트 컨트롤에서 선택된 아이템이 변경될 때 호출되는 핸들러 함수
{
    UNREFERENCED_PARAMETER(pNMHDR);

    POSITION pos = m_listResult.GetFirstSelectedItemPosition();
    if (pos == NULL)
    {
        m_selectedScrewIndex = -1;
    }
    else
    {
        m_selectedScrewIndex = m_listResult.GetNextSelectedItem(pos);
    }

    UpdateSelectedScrewDisplay();
    *pResult = 0;
}

void CMFCSCREWDlg::OnBnClickedBtnSave() // "저장" 버튼 클릭 핸들러 함수
{
    if (!m_lastResult.success || m_lastResult.overlay_image.empty())
    {
        AfxMessageBox(_T("저장할 결과가 없습니다."));
        return;
    }

    CFileDialog dlg(
        FALSE,
        _T("zip"),
        _T("result.zip"),
        OFN_OVERWRITEPROMPT,
        _T("ZIP Files (*.zip)|*.zip||")
    );

    if (dlg.DoModal() != IDOK)
        return;

    CString zipPath = dlg.GetPathName();

    CString baseName = ReplaceExtension(zipPath, _T(""));
    CString tempFolder = baseName + _T("_temp");
    CString imagePath = tempFolder + _T("\\result.png");
    CString csvPath = tempFolder + _T("\\result.csv");
    CString headsFolderPath = tempFolder + _T("\\heads");

    DeleteFolderRecursive(tempFolder);

    if (!CreateDirectory(tempFolder, NULL))
    {
        DWORD err = GetLastError();
        if (err != ERROR_ALREADY_EXISTS)
        {
            AfxMessageBox(_T("임시 폴더 생성 실패"));
            return;
        }
    }

    if (!CreateDirectory(headsFolderPath, NULL))
    {
        DWORD err = GetLastError();
        if (err != ERROR_ALREADY_EXISTS)
        {
            DeleteFolderRecursive(tempFolder);
            AfxMessageBox(_T("헤드 폴더 생성 실패"));
            return;
        }
    }

    CT2A imagePathA(imagePath, CP_UTF8);

    bool imageSaved = cv::imwrite(std::string(imagePathA), m_lastResult.overlay_image);
    bool csvSaved = SaveResultCsv(csvPath);
    bool headsSaved = SaveHeadImages(headsFolderPath);

    if (!(imageSaved && csvSaved && headsSaved))
    {
        DeleteFolderRecursive(tempFolder);

        CString msg;
        msg.Format(_T("저장 실패\n이미지:%s\nCSV:%s\n헤드:%s"),
            imageSaved ? _T("성공") : _T("실패"),
            csvSaved ? _T("성공") : _T("실패"),
            headsSaved ? _T("성공") : _T("실패"));
        AfxMessageBox(msg);
        return;
    }

    bool zipSaved = CreateZipFromFolder(tempFolder, zipPath);
    DeleteFolderRecursive(tempFolder);

    if (zipSaved)
    {
        CString msg;
        msg.Format(_T("ZIP 저장 완료\n%s"), zipPath);
        AfxMessageBox(msg);
    }
    else
    {
        AfxMessageBox(_T("ZIP 압축 실패"));
    }
}