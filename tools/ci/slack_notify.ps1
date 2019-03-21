$body = @{
  username = "AppVeyor CI"
  icon_url = "https://ci.appveyor.com/assets/images/appveyor-blue-144.png"
  attachments = @(
    @{
      fallback = "RDMnet build $Env:RDMNET_VERSION has been deployed to <https://bintray.com/etclabs/rdmnet_bin/latest/$Env:RDMNET_VERSION|Bintray>."
      color = "#5FE35F"
      text = "RDMnet build $Env:RDMNET_VERSION has been deployed to <https://bintray.com/etclabs/rdmnet_bin/latest/$Env:RDMNET_VERSION|Bintray>."
      mrkdwn_in = @(
        "text"
      )
    }
  )
}

$json_body = $body | ConvertTo-Json -Depth 4
Invoke-RestMethod -Uri "$Env:SLACK_WEBHOOK" -Body $json_body -Method POST