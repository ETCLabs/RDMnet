$client = new-object System.Net.WebClient
$client.DownloadFile("http://download.qt-project.org/official_releases/qt/5.9/5.9.7/qt-opensource-windows-x86-5.9.7.exe", "install-qt-5.9.7.exe")
