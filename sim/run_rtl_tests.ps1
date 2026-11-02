param(
 [Parameter(Mandatory=$true)][string]$VivadoBin,
 [string]$Python='python',
 [string]$OutputDir=''
)
$ErrorActionPreference='Stop'
$sourceRoot=Split-Path -Parent $PSScriptRoot
if(-not $OutputDir){$OutputDir=Join-Path $sourceRoot 'generated/rtl_tests'}
if(Test-Path -LiteralPath $OutputDir){throw 'Choose a fresh output directory'}
New-Item -ItemType Directory -Path $OutputDir | Out-Null
$rtl=Join-Path $sourceRoot 'src/fpga/rtl'
$benches=Join-Path $sourceRoot 'sim/rtl'
$cases=@(
 @('tb_iq_split4','IQ_SPLIT4_TEST_PASS'),
 @('tb_dac_stream_guard','DAC_STREAM_GUARD_UNIT_PASS'),
 @('tb_sa_quality_control','SA_QUALITY_CONTROL_PASS'),
 @('tb_sa_frontend_control','SA_FRONTEND_CONTROL_PASS'),
 @('tb_sa_snapshot','SA_SNAPSHOT_SIM_PASS'),
 @('tb_sa_result_control','SA_RESULT_CONTROL_PASS')
)
foreach($case in $cases){
 $testName=$case[0];$marker=$case[1]
 $testOut=Join-Path $OutputDir $testName
 New-Item -ItemType Directory -Path $testOut | Out-Null
 Copy-Item -LiteralPath (Join-Path $rtl 'sa_window_metrics.vh') -Destination $testOut
 Get-ChildItem -LiteralPath (Join-Path $sourceRoot 'src/fpga/coeff/sa') -Filter '*.mem' | Copy-Item -Destination $testOut
 $inputs=@(Get-ChildItem -LiteralPath $rtl -Filter '*.v' | ForEach-Object {$_.FullName})
 $inputs+=Join-Path $benches ($testName+'.v')
 $inputs+=Join-Path $VivadoBin '../data/verilog/src/glbl.v'
 Push-Location $testOut
 try{
  & "$VivadoBin/xvlog.bat" @inputs *> compile.log
  if($LASTEXITCODE -ne 0){throw "Compile failed: $testName (see $testOut)"}
  & "$VivadoBin/xelab.bat" -mt 4 -L xpm $testName work.glbl -s test_snapshot *> elaborate.log
  if($LASTEXITCODE -ne 0){throw "Elaboration failed: $testName"}
  & "$VivadoBin/xsim.bat" test_snapshot -runall -log simulation.log *> console.log
  $result=Get-Content -LiteralPath simulation.log -Raw
  if($LASTEXITCODE -ne 0 -or $result -notmatch $marker -or $result -match 'ERROR:|Fatal:|Error:'){
   throw "Simulation failed: $testName"
  }
  Write-Output "PASS $testName : $marker"
 }finally{Pop-Location}
}
& $Python (Join-Path $PSScriptRoot 'python/verify_sa_snapshot_sim.py') (Join-Path $OutputDir 'tb_sa_snapshot')
if($LASTEXITCODE -ne 0){throw 'Independent snapshot payload/corruption checks failed'}
Write-Output 'V16_SIX_VERILOG_REGRESSIONS_PASS; no board actions'
