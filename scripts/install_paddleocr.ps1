<#
	PaddleOCR 리소스 준비 스크립트 (정보용)

	FastDeploy 경로 자동 설치 스크립트는 2025-11-18부터 사용하지 않습니다.
	대신 Paddle Inference SDK + cpp_infer 모델을 직접 다운로드하고
	CMake 옵션 `TORIYOMI_PADDLE_DIR`, `TORIYOMI_PADDLE_RUNTIME_DIR`를 지정하세요.

	자세한 단계는 README.md / BUILD.md / QUICKSTART.md를 참고하세요.
#>

Write-Host "이 스크립트는 더 이상 FastDeploy를 설치하지 않습니다." -ForegroundColor Yellow
Write-Host "Paddle Inference SDK + PaddleOCR 모델을 수동으로 구성해 주세요." -ForegroundColor Yellow
Write-Host "가이드: README.md > 'Paddle Inference SDK + PaddleOCR 모델 준비' 절" -ForegroundColor Cyan

Write-Host "\n요약:" -ForegroundColor Gray
Write-Host " 1. https://www.paddlepaddle.org.cn/inference/download 에서 Windows CPU SDK 다운로드" -ForegroundColor Gray
Write-Host " 2. SDK 압축을 C:/Dev/paddle_inference와 같은 위치에 풀기" -ForegroundColor Gray
Write-Host " 3. PaddleOCR 모델(det/rec/ppocr_keys_v1.txt)을 models/paddleocr 에 배치" -ForegroundColor Gray
Write-Host " 4. CMake 구성 시 -DTORIYOMI_PADDLE_DIR, -DTORIYOMI_PADDLE_RUNTIME_DIR 옵션 지정" -ForegroundColor Gray

Write-Host "\n필요 시 직접 자동화 스크립트를 작성해 해당 옵션만 맞춰주시면 됩니다." -ForegroundColor DarkGray
