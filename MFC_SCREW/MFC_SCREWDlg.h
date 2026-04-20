#pragma once
#include "afxdialogex.h"
#include "screw_processor.h"
#include <memory>
#include <opencv2/opencv.hpp>

class CMFCSCREWDlg : public CDialogEx
{
public:
    CMFCSCREWDlg(CWnd* pParent = nullptr);

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_SCREWFINDERMFC_DIALOG };
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX);  
    virtual BOOL OnInitDialog();

    DECLARE_MESSAGE_MAP()

private:
    CStatic m_picResult;
    CListCtrl m_listResult;
    CImageList m_imageList;

    CString m_currentImagePath;
    std::unique_ptr<ScrewProcessor> m_processor;
    ProcessResult m_lastResult;
    cv::Mat m_displayImage;

    int m_selectedScrewIndex = -1;

    void ShowImageInControl(CStatic& ctrl, const cv::Mat& img);
    void AdjustListColumns();
    void UpdateSelectedScrewDisplay();

    bool SaveResultCsv(const CString& csvPath);
    bool SaveHeadImages(const CString& headsFolderPath);
    bool CreateZipFromFolder(const CString& sourceFolderPath, const CString& zipPath);
    bool DeleteFolderRecursive(const CString& folderPath);
    CString ReplaceExtension(const CString& path, const CString& newExt);

public:
    afx_msg void OnBnClickedBtnOpen();
    afx_msg void OnBnClickedBtnRun();
    afx_msg void OnBnClickedBtnSave();
    afx_msg void OnLvnItemchangedListResult(NMHDR* pNMHDR, LRESULT* pResult);
};