# The launcher window's layout (22 September): a menu of five pages on the
# left with the versions and the coffee under it, the actions as cards on the
# page, the status and a log that folds away on every page, and a painted
# background. PowerShell 5.1 / WinForms.
#
# Metin2-Launcher-GUI.ps1 dot-sources this AFTER every original Click handler
# and BEFORE the timers and ShowDialog, and only when the file is there. It
# builds no action of its own: it moves the controls the window already has
# into the new panels, so their handlers and the one-action-at-a-time guard
# (Start-LauncherAction) stay exactly as they were. The one new button is
# the rates, which opens the classic panel's /rates.
#
# Nothing the player can see changes before $script:ui.Cleared is set, so a
# failure above that line leaves the plain window; a failure below it makes
# the GUI write .m2launcher-classic-layout and ask for a restart, which then
# opens the plain window (see the dot-source in Metin2-Launcher-GUI.ps1).

# Native background renderer: proportional cover, right alignment and a contrast
# veil. The artwork stays unchanged on disk.
#
# The picture is composed once per size into a bitmap and every paint copies
# its own rectangle of that bitmap 1:1. It used to be scaled from 1536 x 1024
# with a bicubic filter on every paint - and almost every control above it has
# a transparent or half-transparent background, which WinForms paints by asking
# the parent for its background, so one step of a window resize scaled the
# whole picture some forty times: 378 ms a step, the lag players saw. Between
# the form's ResizeBegin and ResizeEnd the bitmap is made with a cheap filter,
# and the fine one once when the mouse is let go. A new class name, because a
# type compiled by Add-Type lives as long as its process.
if (-not ('M2LauncherArtPanel2' -as [type])) {
    try {
    Add-Type -ReferencedAssemblies System.Windows.Forms,System.Drawing -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Windows.Forms;
public class M2LauncherArtPanel2 : Panel {
    private Image scene;
    private Bitmap composed;
    private bool composedRough;
    private bool liveResize;
    public M2LauncherArtPanel2() {
        SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer |
            ControlStyles.UserPaint | ControlStyles.ResizeRedraw, true);
    }
    public Image SceneImage {
        get { return scene; }
        set { scene = value; DropComposed(); Invalidate(); }
    }
    public bool LiveResize {
        get { return liveResize; }
        set {
            if (liveResize == value) return;
            liveResize = value;
            if (!liveResize && composedRough) { DropComposed(); Invalidate(true); }
        }
    }
    private void DropComposed() {
        if (composed != null) { composed.Dispose(); composed = null; }
    }
    protected override void OnSizeChanged(EventArgs e) {
        DropComposed();
        base.OnSizeChanged(e);
    }
    private Bitmap Composed() {
        int width = ClientSize.Width, height = ClientSize.Height;
        if (width < 1 || height < 1) return null;
        if (composed != null && composed.Width == width && composed.Height == height) return composed;
        DropComposed();
        composed = new Bitmap(width, height, PixelFormat.Format32bppPArgb);
        composedRough = liveResize;
        using (Graphics g = Graphics.FromImage(composed)) {
            g.Clear(BackColor);
            if (scene != null) {
                float scale = Math.Max((float)width / scene.Width, (float)height / scene.Height);
                float w = scene.Width * scale, h = scene.Height * scale;
                g.InterpolationMode = liveResize ? InterpolationMode.Bilinear : InterpolationMode.HighQualityBicubic;
                g.PixelOffsetMode = liveResize ? PixelOffsetMode.HighSpeed : PixelOffsetMode.HighQuality;
                g.DrawImage(scene, new RectangleF(width - w, (height - h) / 2, w, h));
            }
            Rectangle all = new Rectangle(0, 0, width, height);
            using (LinearGradientBrush veil = new LinearGradientBrush(all,
                Color.FromArgb(150, 9, 15, 19), Color.FromArgb(12, 9, 15, 19), 0f)) {
                g.FillRectangle(veil, all);
            }
        }
        return composed;
    }
    protected override void OnPaintBackground(PaintEventArgs e) {
        Bitmap picture = Composed();
        if (picture == null) { base.OnPaintBackground(e); return; }
        // A transparent child's request arrives with the graphics shifted to
        // the child and the clip in this panel's coordinates.
        Rectangle area = Rectangle.Intersect(e.ClipRectangle, new Rectangle(0, 0, picture.Width, picture.Height));
        if (area.Width <= 0 || area.Height <= 0) return;
        e.Graphics.CompositingMode = CompositingMode.SourceCopy;
        e.Graphics.InterpolationMode = InterpolationMode.NearestNeighbor;
        e.Graphics.DrawImage(picture, area, area, GraphicsUnit.Pixel);
    }
    protected override void Dispose(bool disposing) {
        if (disposing) {
            DropComposed();
            if (scene != null) { scene.Dispose(); scene = null; }
        }
        base.Dispose(disposing);
    }
}
'@
    }
    catch {
        # Add-Type compiles with csc.exe in %TEMP%; an antivirus or a locked
        # %TEMP% can refuse that, and it must cost the painting, not the
        # window: the panel below is then a plain one with the picture
        # stretched to it, or a plain colour.
        Write-StartupFailure ('Tlo launchera bez rysowania C#: ' + $_.Exception.Message)
    }
}

function UI-Text([string]$Pl, [string]$En) {
    if ($script:Lang -eq 'en') { return $En }
    return $Pl
}
function UI-Color([string]$Hex) { return [Drawing.ColorTranslator]::FromHtml($Hex) }
$script:ui = @{
    Background = (UI-Color '#10161A'); Surface = (UI-Color '#182125')
    Raised = (UI-Color '#273035'); Gold = (UI-Color '#E7BF73')
    Muted = (UI-Color '#B2B9B9'); Text = (UI-Color '#F4EFE5')
    Pages = @{}; Nav = @{}; Cards = @(); LogExpanded = $true
}
function UI-Label($Parent, [string]$Text, [float]$Size, $Color, [int]$Height) {
    $label = [Windows.Forms.Label]::new()
    $label.Text = $Text
    $label.Font = [Drawing.Font]::new('Segoe UI', $Size)
    $label.ForeColor = $Color
    $label.BackColor = [Drawing.Color]::Transparent
    $label.Dock = 'Top'; $label.Height = $Height
    $label.AutoEllipsis = $true
    $Parent.Controls.Add($label)
    return $label
}
function UI-ButtonStyle($Button) {
    $Button.Font = [Drawing.Font]::new('Segoe UI Semibold', 10)
    $Button.BackColor = $script:ui.Raised
    $Button.ForeColor = $script:ui.Text
    $Button.FlatStyle = 'Flat'; $Button.FlatAppearance.BorderSize = 1
    $Button.FlatAppearance.BorderColor = UI-Color '#505044'
    $Button.FlatAppearance.MouseOverBackColor = UI-Color '#414136'
    $Button.FlatAppearance.MouseDownBackColor = UI-Color '#55503C'
    $Button.Cursor = [Windows.Forms.Cursors]::Hand
    $Button.AutoEllipsis = $true
    $Button.UseMnemonic = $false
}
function UI-Page([string]$Id, [string]$Heading, [string]$Description) {
    $page = [Windows.Forms.Panel]::new()
    $page.Dock = 'Fill'; $page.AutoScroll = $true
    $page.BackColor = [Drawing.Color]::Transparent
    $page.Padding = [Windows.Forms.Padding]::new(28, 16, 28, 12)
    $grid = [Windows.Forms.TableLayoutPanel]::new()
    $grid.Dock = 'Top'; $grid.Height = 348
    $grid.BackColor = [Drawing.Color]::Transparent
    $grid.ColumnCount = 2; $grid.RowCount = 3
    [void]$grid.ColumnStyles.Add([Windows.Forms.ColumnStyle]::new([Windows.Forms.SizeType]::Percent, 50))
    [void]$grid.ColumnStyles.Add([Windows.Forms.ColumnStyle]::new([Windows.Forms.SizeType]::Percent, 50))
    for ($i = 0; $i -lt 3; $i++) {
        [void]$grid.RowStyles.Add([Windows.Forms.RowStyle]::new([Windows.Forms.SizeType]::Percent, 33.333))
    }
    $page.Controls.Add($grid)
    $descriptionLabel = UI-Label $page $Description 10 $script:ui.Muted 40
    $headingLabel = UI-Label $page $Heading 23 $script:ui.Text 46
    $script:ui.Content.Controls.Add($page)
    $script:ui.Pages[$Id] = @{ Panel = $page; Grid = $grid; Count = 0; Heading = $headingLabel; Description = $descriptionLabel }
    $page.Tag = $Id
    $page.Add_SizeChanged({
        $entry = $script:ui.Pages[$this.Tag]
        if ($entry) {
            if ($this.Tag -eq 'home') { $entry.Grid.Height = 368 }
            else { $entry.Grid.Height = [Math]::Max(276, [Math]::Min(348, $this.ClientSize.Height - 114)) }
        }
    })
    $page.Visible = $false
}
function UI-Card([string]$PageId, $Button, [string]$Description, [string]$Accent = '') {
    if (-not $Button) { return }
    $page = $script:ui.Pages[$PageId]
    $card = [Windows.Forms.Panel]::new()
    $card.Dock = 'Fill'; $card.Margin = [Windows.Forms.Padding]::new(0, 0, 12, 8)
    $card.Padding = [Windows.Forms.Padding]::new(14, 8, 14, 6)
    $card.BackColor = [Drawing.Color]::FromArgb(232, 18, 26, 30)
    $card.Add_Paint({
        param($sender, $eventArgs)
        $edge = [Drawing.Pen]::new([Drawing.Color]::FromArgb(65, 198, 158, 90))
        try { $eventArgs.Graphics.DrawRectangle($edge, 0, 0, ($sender.Width - 1), ($sender.Height - 1)) }
        finally { $edge.Dispose() }
    })
    UI-ButtonStyle $Button
    $Button.Dock = 'Top'; $Button.Height = 36
    $Button.TextAlign = 'MiddleLeft'; $Button.Padding = [Windows.Forms.Padding]::new(10, 0, 8, 0)
    if ($Accent) {
        $Button.BackColor = UI-Color $Accent
        $Button.FlatAppearance.MouseOverBackColor = [Windows.Forms.ControlPaint]::Light($Button.BackColor)
    }
    $hint = UI-Label $card $Description 9 $script:ui.Muted 43
    $hint.Dock = 'Fill'; $hint.Padding = [Windows.Forms.Padding]::new(2, 6, 0, 0)
    $card.Controls.Add($Button)
    $column = $page.Count % 2; $row = [int][Math]::Floor($page.Count / 2)
    $page.Grid.Controls.Add($card, $column, $row)
    $page.Count++
    $script:ui.Cards += @{ Page = $PageId; Button = $Button; Card = $card; Hint = $hint }
}
function Show-UIPage([string]$Id) {
    foreach ($key in $script:ui.Pages.Keys) {
        $script:ui.Pages[$key].Panel.Visible = ($key -eq $Id)
        $script:ui.Nav[$key].BackColor = if ($key -eq $Id) { $script:ui.Raised } else { UI-Color '#0D141A' }
        $script:ui.Nav[$key].ForeColor = if ($key -eq $Id) { $script:ui.Gold } else { $script:ui.Muted }
        $script:ui.Nav[$key].FlatAppearance.BorderColor = if ($key -eq $Id) { UI-Color '#897247' } else { UI-Color '#0D141A' }
    }
    $script:ui.Pages[$Id].Panel.BringToFront()
    $script:ui.CurrentPage = $Id
}
function Switch-UILog {
    $script:ui.LogExpanded = -not $script:ui.LogExpanded
    $script:ui.LogPanel.Height = if ($script:ui.LogExpanded) { 164 } else { 52 }
    $script:logBox.Visible = $script:ui.LogExpanded
    $script:ui.LogToggle.Text = if ($script:ui.LogExpanded) { UI-Text 'Ukryj log' 'Hide log' } else { UI-Text 'Pokaż log' 'Show log' }
}

$script:form.SuspendLayout()
# Reparent existing controls; their handlers and the original action guard remain intact.
$script:ui.Cleared = $true
$script:form.Controls.Clear()
$script:form.AutoScaleMode = 'None'
# 1280 x 820 where the screen has room, and never more than the screen: a
# laptop of 1366 x 768, or 1920 x 1080 at 150% scaling, has 720 to 740
# pixels of height, and the pages scroll.
$workArea = [Windows.Forms.Screen]::PrimaryScreen.WorkingArea
$script:form.MinimumSize = [Drawing.Size]::new([Math]::Min(1020, $workArea.Width), [Math]::Min(780, $workArea.Height))
$script:form.ClientSize = [Drawing.Size]::new([Math]::Min(1280, $workArea.Width - 16), [Math]::Min(820, $workArea.Height - 40))
$script:form.BackColor = $script:ui.Background
$script:form.ForeColor = $script:ui.Text

$body = [Windows.Forms.Panel]::new(); $body.Dock = 'Fill'
$script:form.Controls.Add($body)
$header = [Windows.Forms.Panel]::new(); $header.Dock = 'Top'; $header.Height = 90
$header.BackColor = UI-Color '#0D141A'; $header.Padding = [Windows.Forms.Padding]::new(26, 14, 26, 10)
$script:form.Controls.Add($header)
$brand = [Windows.Forms.Panel]::new(); $brand.Dock = 'Fill'; $header.Controls.Add($brand)
$subtitle.Text = 'SINGLEPLAYER  /  PLAYERBOTS  /  BY TIERU'
$subtitle.Dock = 'Top'; $subtitle.Height = 24
$subtitle.Font = [Drawing.Font]::new('Segoe UI', 9); $subtitle.ForeColor = $script:ui.Muted
$brand.Controls.Add($subtitle)
$title.Text = 'METIN2'; $title.Dock = 'Top'; $title.Height = 37; $title.Font = [Drawing.Font]::new('Georgia', 25, [Drawing.FontStyle]::Bold)
$title.ForeColor = $script:ui.Gold; $brand.Controls.Add($title)
$brand.Controls.SetChildIndex($title, 0)
$website = New-Button 'metin2singleplayer.com  >' 0 0 240 44
UI-ButtonStyle $website
$website.Dock = 'Right'; $website.Width = 240; $website.BackColor = UI-Color '#19232C'
$website.ForeColor = $script:ui.Gold
$website.AccessibleName = UI-Text 'Oficjalna strona projektu' 'Official project website'
$website.Add_Click({ Start-Process 'https://metin2singleplayer.com/' })
$header.Controls.Add($website)
UI-ButtonStyle $languageButton
$languageButton.Text = if ($script:Lang -eq 'en') { 'EN / PL' } else { 'PL / EN' }
$languageButton.Dock = 'Right'; $languageButton.Width = 86
$languageButton.AccessibleName = UI-Text 'Zmień język launchera' 'Change launcher language'
$header.Controls.Add($languageButton)
$script:ui.Cards += @{ Page = 'header'; Button = $languageButton; Card = $header; Hint = $null }

$artPanel = 'M2LauncherArtPanel2' -as [type]
if ($artPanel) { $main = $artPanel::new() }
else { $main = [Windows.Forms.Panel]::new(); $main.BackgroundImageLayout = 'Zoom' }
$main.Dock = 'Fill'; $body.Controls.Add($main)
# A drag of the window's edge. Laying the pages out is some 75 ms of every
# step even with the picture composed once (a hundred controls, the table
# grids, the ellipsised captions), so the cards keep their places while the
# edge moves - the picture follows the window at once - and are laid out once
# when the mouse is let go. A plain move raises the same two events and asks
# for no layout, so ResumeLayout then does nothing. Maximise and restore raise
# neither and are laid out in one pass.
function Start-UILiveResize {
    if ($script:ui.LiveResize) { return }
    $script:ui.LiveResize = $true
    if ($main.PSObject.Properties['LiveResize']) { $main.LiveResize = $true }
    $main.SuspendLayout()
}
function Stop-UILiveResize {
    if (-not $script:ui.LiveResize) { return }
    $script:ui.LiveResize = $false
    $main.ResumeLayout($true)
    if ($main.PSObject.Properties['LiveResize']) { $main.LiveResize = $false }
}
$script:form.Add_ResizeBegin({ Start-UILiveResize })
$script:form.Add_ResizeEnd({ Stop-UILiveResize })
$scenePath = Join-Path $PSScriptRoot 'Metin2-Launcher-GUI.Background.png'
if (Test-Path -LiteralPath $scenePath -PathType Leaf) {
    try {
        # Clone the decoded bitmap to release the source file immediately:
        # an update replaces it while the window is open.
        $sceneSource = [Drawing.Image]::FromFile($scenePath)
        try {
            if ($artPanel) { $main.SceneImage = [Drawing.Bitmap]::new($sceneSource) }
            else { $main.BackgroundImage = [Drawing.Bitmap]::new($sceneSource) }
        }
        finally { $sceneSource.Dispose() }
    } catch { Write-LocalLog ('Nie można wczytać tła launchera: ' + $_.Exception.Message) }
}
function Scroll-UILogToEnd {
    if ($script:logBox -and -not $script:logBox.IsDisposed -and $script:logBox.IsHandleCreated -and $script:logBox.Visible) {
        $script:logBox.SelectionStart = $script:logBox.TextLength
        $script:logBox.SelectionLength = 0
        $script:logBox.ScrollToCaret()
    }
}
function Get-UIRatesUrl {
    $addresses = Get-M2PanelAddresses -ServerRoot $root
    return ([Uri]::new([Uri]$addresses.ClassicUrl, '/rates')).AbsoluteUri
}
$sidebar = [Windows.Forms.Panel]::new(); $sidebar.Dock = 'Left'; $sidebar.Width = 252
$sidebar.BackColor = UI-Color '#0D141A'; $sidebar.Padding = [Windows.Forms.Padding]::new(10, 16, 10, 14)
$body.Controls.Add($sidebar); $script:ui.Sidebar = $sidebar
$navStack = [Windows.Forms.FlowLayoutPanel]::new()
$navStack.Dock = 'Top'; $navStack.Height = 300
$navStack.FlowDirection = 'TopDown'; $navStack.WrapContents = $false
$sidebar.Controls.Add($navStack)
$coffeeButton = New-Button (UI-Text '☕  Postaw kawkę' '☕  Buy a coffee') 0 0 232 44
UI-ButtonStyle $coffeeButton
$coffeeButton.Dock = 'Bottom'; $coffeeButton.Height = 44
$coffeeButton.Font = [Drawing.Font]::new('Segoe UI Semibold', 10)
$coffeeButton.ForeColor = $script:ui.Gold
$coffeeButton.AccessibleName = UI-Text 'Wesprzyj projekt na BuyCoffee' 'Support the project on BuyCoffee'
$coffeeButton.Add_Click({ Start-Process 'https://buycoffee.to/metin2-playerbots' })
$sidebar.Controls.Add($coffeeButton)
$versionPanel = [Windows.Forms.Panel]::new()
$versionPanel.Dock = 'Bottom'; $versionPanel.Height = 232
$versionPanel.Padding = [Windows.Forms.Padding]::new(10, 12, 8, 8)
$versionPanel.BackColor = $script:ui.Surface
$sidebar.Controls.Add($versionPanel)
$script:ui.SideVersions = [Windows.Forms.Label]::new()
$script:ui.SideVersions.Dock = 'Fill'
$script:ui.SideVersions.Font = [Drawing.Font]::new('Segoe UI', 9)
$script:ui.SideVersions.ForeColor = $script:ui.Muted
$versionPanel.Controls.Add($script:ui.SideVersions)
$null = UI-Label $versionPanel (UI-Text 'WERSJE' 'VERSIONS') 10 $script:ui.Gold 30
$script:versionLabel.Add_TextChanged({
    $script:ui.SideVersions.Text = $script:versionLabel.Text -replace '\s+\|\s+', "`r`n"
})
$script:versionLabel.Add_ForeColorChanged({ $script:ui.SideVersions.ForeColor = $script:versionLabel.ForeColor })

$content = [Windows.Forms.Panel]::new(); $content.Dock = 'Fill'; $main.Controls.Add($content)
$content.BackColor = [Drawing.Color]::Transparent
$content.Add_SizeChanged({
    # Reserve a quiet illustration area only when the action cards have room.
    $artSpace = [Math]::Max(0, [Math]::Min(220, $this.ClientSize.Width - 740))
    $this.Padding = [Windows.Forms.Padding]::new(0, 0, $artSpace, 0)
})
$script:ui.Content = $content
$statusStrip = [Windows.Forms.TableLayoutPanel]::new()
$statusStrip.Dock = 'Top'; $statusStrip.Height = 57
$statusStrip.BackColor = [Drawing.Color]::Transparent
$statusStrip.Padding = [Windows.Forms.Padding]::new(28, 14, 28, 6)
$statusStrip.ColumnCount = 2; $statusStrip.RowCount = 1
[void]$statusStrip.ColumnStyles.Add([Windows.Forms.ColumnStyle]::new([Windows.Forms.SizeType]::Percent, 50))
[void]$statusStrip.ColumnStyles.Add([Windows.Forms.ColumnStyle]::new([Windows.Forms.SizeType]::Percent, 50))
foreach ($status in @($script:dockerStatus, $script:serverStatus)) {
    $status.Dock = 'Fill'; $status.Padding = [Windows.Forms.Padding]::new(12, 7, 0, 0)
    $status.BackColor = $script:ui.Surface; $status.Font = [Drawing.Font]::new('Segoe UI Semibold', 9)
    $status.Text = '...'; $status.ForeColor = $script:ui.Muted
    $statusStrip.Controls.Add($status)
}
$main.Controls.Add($statusStrip)

$logPanel = [Windows.Forms.Panel]::new(); $logPanel.Dock = 'Bottom'; $logPanel.Height = 164
$logPanel.Padding = [Windows.Forms.Padding]::new(28, 4, 28, 12)
$logPanel.BackColor = [Drawing.Color]::FromArgb(230, 10, 16, 20)
$main.Controls.Add($logPanel); $script:ui.LogPanel = $logPanel
$script:logBox.Dock = 'Fill'; $script:logBox.BorderStyle = 'None'
$script:logBox.BackColor = UI-Color '#0D141A'; $script:logBox.ForeColor = UI-Color '#B6C5CF'
$script:logBox.Font = [Drawing.Font]::new('Consolas', 9)
$script:logBox.Add_TextChanged({ Scroll-UILogToEnd })
$script:logBox.Add_VisibleChanged({
    if ($script:logBox.Visible -and $script:form.IsHandleCreated) {
        [void]$script:form.BeginInvoke([Action]{ Scroll-UILogToEnd })
    }
})
$script:form.Add_Shown({ [void]$script:form.BeginInvoke([Action]{ Scroll-UILogToEnd }) })
$logPanel.Controls.Add($script:logBox)
$script:progress.Dock = 'Top'; $script:progress.Height = 4; $logPanel.Controls.Add($script:progress)
$logHeader = [Windows.Forms.Panel]::new(); $logHeader.Dock = 'Top'; $logHeader.Height = 32
$logHeader.BackColor = [Drawing.Color]::Transparent
$logPanel.Controls.Add($logHeader)
$script:actionStatus.Dock = 'Fill'; $script:actionStatus.ForeColor = $script:ui.Gold
$script:actionStatus.BackColor = [Drawing.Color]::Transparent
$script:actionStatus.Padding = [Windows.Forms.Padding]::new(0, 6, 0, 0)
$logHeader.Controls.Add($script:actionStatus)
$script:ui.LogToggle = New-Button (UI-Text 'Ukryj log' 'Hide log') 0 0 110 28
UI-ButtonStyle $script:ui.LogToggle
$script:ui.LogToggle.Dock = 'Right'; $script:ui.LogToggle.Width = 110
$script:ui.LogToggle.Font = [Drawing.Font]::new('Segoe UI', 9)
$script:ui.LogToggle.Add_Click({ Switch-UILog })
$logHeader.Controls.Add($script:ui.LogToggle)
$footer.Dock = 'Bottom'; $footer.Height = 34
$footer.BackColor = [Drawing.Color]::FromArgb(240, 10, 16, 20)
$footer.Text = UI-Text 'Twój świat. Twoje tempo.    •    Metin2 Singleplayer by Tieru' 'Your world. Your pace.    •    Metin2 Singleplayer by Tieru'
$footer.ForeColor = $script:ui.Muted; $footer.Padding = [Windows.Forms.Padding]::new(28, 6, 0, 0)
$main.Controls.Add($footer)

$sections = @(
    @('home', '01', (UI-Text 'Pulpit' 'Overview'), (UI-Text 'Wróć do swojego świata' 'Return to your world'), (UI-Text 'Uruchom rozgrywkę lub zarządzaj działającym serwerem.' 'Start playing or manage your running server.')),
    @('world', '02', (UI-Text 'Świat i boty' 'World & bots'), (UI-Text 'Świat na Twoich zasadach' 'A world on your terms'), (UI-Text 'Ustaw boty i poziom trudności.' 'Configure bots and difficulty.')),
    @('coop', '03', (UI-Text "COOP`r`n      (DLA WSPIERAJĄCYCH)" "COOP`r`n      (FOR SUPPORTERS)"), (UI-Text 'Graj razem ze znajomymi' 'Play together with friends'), (UI-Text 'COOP dla wspierających — zarządzaj wspólną rozgrywką.' 'COOP for supporters — manage your shared adventure.')),
    @('database', '04', (UI-Text 'Baza danych' 'Database'), (UI-Text 'Zarządzanie bazą danych' 'Database management'), (UI-Text 'Dostęp, import i kopie Twojego świata.' 'Connection details, imports and backups of your world.')),
    @('logs', '05', (UI-Text 'Logi i diagnostyka' 'Logs & diagnostics'), (UI-Text 'Sprawdź, co się dzieje' 'See what is happening'), (UI-Text 'Diagnostyka i materiały potrzebne do zgłoszenia problemu.' 'Diagnostics and the information needed to report a problem.'))
)
foreach ($section in $sections) {
    UI-Page $section[0] $section[3] $section[4]
    $nav = New-Button ($section[1] + '   ' + $section[2]) 0 0 232 48
    if ($section[0] -eq 'coop') { $nav.Height = 64 }
    UI-ButtonStyle $nav; $nav.TextAlign = 'MiddleLeft'
    $nav.Padding = [Windows.Forms.Padding]::new(10, 0, 0, 0)
    $nav.Margin = [Windows.Forms.Padding]::new(0, 5, 0, 0)
    $nav.Tag = @{ Id = $section[0]; Short = $section[1]; Title = $nav.Text }
    $nav.AccessibleName = $section[2]
    $nav.Add_Click({ Show-UIPage $this.Tag.Id })
    $navStack.Controls.Add($nav); $script:ui.Nav[$section[0]] = $nav
}
$sidebar.Add_SizeChanged({
    foreach ($control in $navStack.Controls) { $control.Width = [Math]::Max(40, $sidebar.ClientSize.Width - 20) }
})

# Change the translation source too: toggling launch-client refreshes this label.
foreach ($uiLocale in @('pl', 'en')) {
    foreach ($key in @('play', 'playNoClient')) {
        $script:Strings[$uiLocale][$key] = $script:Strings[$uiLocale][$key] -replace '^\s*2\.\s*', ''
    }
}
Update-PlayButtonLabel
$dockerButton.Visible = $false
$homeGrid = $script:ui.Pages.home.Grid
$homeGrid.RowCount = 4; $homeGrid.RowStyles.Clear(); $homeGrid.Height = 368
for ($i = 0; $i -lt 4; $i++) {
    [void]$homeGrid.RowStyles.Add([Windows.Forms.RowStyle]::new([Windows.Forms.SizeType]::Percent, 25))
}
$script:ui.Pages.home.Heading.Height = 40
$script:ui.Pages.home.Description.Height = 36
UI-Card 'home' $playButton (UI-Text 'Uruchom serwer i rozpocznij przygodę.' 'Start the server and begin your adventure.') '#21694F'
UI-Card 'home' $stopButton (UI-Text 'Bezpieczne zatrzymanie z zachowaniem postępu.' 'Stop safely and keep your progress.') '#6A3C39'
UI-Card 'home' $panelButton (UI-Text 'Statystyki, rankingi i panel zarządzania.' 'Statistics, rankings and management panel.')
$launchPanel = [Windows.Forms.Panel]::new(); $launchPanel.Dock = 'Fill'
$launchPanel.BackColor = [Drawing.Color]::Transparent
$launchPanel.Padding = [Windows.Forms.Padding]::new(4, 6, 0, 0)
$script:launchClientCheck.Dock = 'Top'; $script:launchClientCheck.Height = 30
$script:launchClientCheck.Font = [Drawing.Font]::new('Segoe UI', 10)
$script:launchClientCheck.ForeColor = $script:ui.Text
$script:launchClientCheck.BackColor = [Drawing.Color]::FromArgb(230, 18, 26, 30)
$launchPanel.Controls.Add($script:launchClientCheck)
$script:ui.Pages.home.Grid.Controls.Add($launchPanel, 1, 3)

UI-Card 'world' $botCountButton (UI-Text 'Liczba botów, królestwa i tempo zaludniania.' 'Bot count, kingdoms and population pace.')
UI-Card 'world' $difficultyButton (UI-Text 'Dostosuj rozwój postaci do swojego tempa.' 'Adjust character progression to your own pace.')
$ratesButton = New-Button (UI-Text 'RATY: EXP / DROP / YANG' 'RATES: EXP / DROP / YANG') 0 0 280 36
$ratesButton.Add_Click({
    $ratesUrl = Get-UIRatesUrl
    Write-LocalLog (UI-Text 'Otwieram edytor rat serwera w panelu WWW.' 'Opening server rates in the web panel.')
    Start-Process $ratesUrl
})
UI-Card 'world' $ratesButton (UI-Text 'Edytuj mnożniki w panelu WWW. Serwer musi działać.' 'Edit multipliers in the web panel. The server must be running.')
UI-Card 'coop' $coopButton (UI-Text 'Zaproś znajomych do wspólnej rozgrywki.' 'Invite friends to play together.')
$coopInfo = [Windows.Forms.Panel]::new()
$coopInfo.Dock = 'Fill'; $coopInfo.BackColor = [Drawing.Color]::FromArgb(235, 18, 26, 30)
$coopInfo.Padding = [Windows.Forms.Padding]::new(14, 8, 14, 8)
$coopInfo.Margin = [Windows.Forms.Padding]::new(0, 0, 12, 0)
$coopText = UI-Label $coopInfo (UI-Text "Hasło otrzymują wspierający — znajdziesz je na Discordzie, na kanale dla wspierających.`r`n`r`nWsparcie opłaca tylko osoba hostująca grę. Zaproszeni gracze nie muszą płacić." "Supporters receive the password on Discord, in the supporters-only channel.`r`n`r`nOnly the person hosting the game needs to pay for support. Invited players do not need to pay.") 10 $script:ui.Text 100
$coopText.Dock = 'Fill'
$script:ui.Pages.coop.Grid.Controls.Add($coopInfo, 0, 1)
$script:ui.Pages.coop.Grid.SetColumnSpan($coopInfo, 2)
$script:ui.Pages.coop.Grid.SetRowSpan($coopInfo, 2)
if (-not $coopButton) {
    $null = UI-Label $script:ui.Pages.coop.Grid (UI-Text 'Moduł COOP nie jest zainstalowany.' 'The COOP module is not installed.') 10 $script:ui.Muted 60
}

UI-Card 'home' $installButton (UI-Text 'Przygotuj pliki i zależności serwera.' 'Prepare server files and dependencies.')
UI-Card 'home' $clientButton (UI-Text 'Wskaż plik uruchamiający klienta gry.' 'Select the game client executable.')
UI-Card 'home' $updateButton (UI-Text 'Sprawdź dostępność nowej wersji projektu.' 'Check for a new project version.')
UI-Card 'home' $gmPanelButton (UI-Text 'Pobierz pakiet klienta dla tej wersji serwera.' 'Get the client package for this server version.')

UI-Card 'database' $dbAccessButton (UI-Text 'Dane połączenia dla Navicat i innych narzędzi.' 'Connection details for Navicat and other tools.')
UI-Card 'database' $repairDbButton (UI-Text 'Przywróć dostęp narzędzi do bazy serwera.' 'Restore database access for external tools.')
UI-Card 'database' $importDbButton (UI-Text 'Przenieś bazę z innej instalacji.' 'Bring a database from another installation.')
UI-Card 'database' $worldBackupButton (UI-Text 'Utwórz kopię, przywróć zapis lub nowy świat.' 'Back up, restore a save or create a new world.')

UI-Card 'logs' $diagnosticsButton (UI-Text 'Sprawdź środowisko i możliwe przyczyny błędów.' 'Check the environment and possible causes of errors.')
UI-Card 'logs' $bundleButton (UI-Text 'Przygotuj paczkę logów do zgłoszenia.' 'Prepare a log bundle for a support request.')
UI-Card 'logs' $openLogButton (UI-Text 'Otwórz bieżący dziennik w edytorze.' 'Open the current log in an editor.')
UI-Card 'logs' $folderButton (UI-Text 'Przejdź do wszystkich zapisanych logów.' 'Browse all saved log files.')

Show-UIPage 'home'
$script:ui.VersionTip = [Windows.Forms.ToolTip]::new()
$script:versionLabel.Add_ForeColorChanged({
    # Keep the existing update notice visible even outside the updates page.
    if ($script:updateAvailable) {
        $footer.Text = UI-Text 'Dostępna aktualizacja — przejdź do pulpitu.' 'Update available — open Overview.'
        $footer.ForeColor = $script:ui.Gold
    } else {
        $footer.Text = UI-Text 'Twój świat. Twoje tempo.    •    Metin2 Singleplayer by Tieru' 'Your world. Your pace.    •    Metin2 Singleplayer by Tieru'
        $footer.ForeColor = $script:ui.Muted
    }
    $script:ui.VersionTip.SetToolTip($script:ui.SideVersions, $script:versionLabel.Text)
})
$script:form.ResumeLayout($true)

function Invoke-LayoutSelfTest([string]$OutputDirectory) {
    # Only layout/navigation are exercised. Exit occurs before any runtime timer,
    # Docker query, update check, original action or server log write.
    [void][IO.Directory]::CreateDirectory($OutputDirectory)
    $script:form.StartPosition = 'Manual'
    $script:form.Location = [Drawing.Point]::new(-30000, -30000)
    $script:form.ShowInTaskbar = $false
    $script:dockerStatus.Text = UI-Text 'Docker: podgląd UI' 'Docker: UI preview'
    $script:serverStatus.Text = UI-Text 'Serwer: podgląd UI' 'Server: UI preview'
    $script:versionLabel.Text = "Serwer: 2.0.96   |   najnowszy: 2.0.96`r`nLauncher: 2.0.96   |   najnowszy: 2.0.96`r`nKlient: 2.0.25   |   najnowszy: 2.0.25"
    $script:logBox.Text = UI-Text "[Test] Podgląd układu launchera.`r`n[Test] Akcje serwera nie zostały uruchomione." "[Test] Launcher layout preview.`r`n[Test] No server actions have been started."
    $previewLog = $script:logBox.Text
    $script:logBox.Text = ((1..100 | ForEach-Object { 'Scroll test line ' + $_ }) -join "`r`n")
    $script:form.Show()
    [Windows.Forms.Application]::DoEvents()
    if ($script:logBox.SelectionStart -ne $script:logBox.TextLength) { throw 'Log caret did not follow text' }
    $lastPosition = $script:logBox.GetPositionFromCharIndex($script:logBox.TextLength)
    if ($lastPosition.Y -lt 0 -or $lastPosition.Y -ge $script:logBox.ClientSize.Height) { throw 'Last log line is outside viewport' }
    $script:logBox.AppendText("`r`nScroll test appended line")
    if ($script:logBox.SelectionStart -ne $script:logBox.TextLength) { throw 'New log line did not scroll' }
    $script:logBox.Text = $previewLog
    # What a drag of the window's edge costs: forty sizes between the same
    # two calls ResizeBegin and ResizeEnd make, each painted whole into a
    # bitmap (the window is off screen, so a plain Refresh would paint
    # nothing), then the one layout the release costs. 378 ms a step before
    # the picture was composed once per size and the pages waited for the
    # release.
    $resizeBitmap = [Drawing.Bitmap]::new(1400, 900)
    $resizeClock = [Diagnostics.Stopwatch]::StartNew()
    Start-UILiveResize
    for ($i = 0; $i -lt 40; $i++) {
        $script:form.ClientSize = [Drawing.Size]::new(1020 + (($i % 20) * 13), 780 + (($i % 20) * 2))
        [Windows.Forms.Application]::DoEvents()
        $script:form.DrawToBitmap($resizeBitmap, [Drawing.Rectangle]::new(0, 0, $script:form.Width, $script:form.Height))
    }
    $resizeClock.Stop()
    $releaseClock = [Diagnostics.Stopwatch]::StartNew()
    Stop-UILiveResize
    [Windows.Forms.Application]::DoEvents()
    $script:form.DrawToBitmap($resizeBitmap, [Drawing.Rectangle]::new(0, 0, $script:form.Width, $script:form.Height))
    $releaseClock.Stop(); $resizeBitmap.Dispose()
    $resizeMsPerStep = [Math]::Round($resizeClock.Elapsed.TotalMilliseconds / 40, 1)
    $releaseMs = [Math]::Round($releaseClock.Elapsed.TotalMilliseconds, 1)
    $results = @()
    foreach ($dimensions in @(@(1280, 820), @(1120, 820), @(1004, 741))) {
        $script:form.ClientSize = [Drawing.Size]::new($dimensions[0], $dimensions[1])
        foreach ($id in @('home', 'world', 'coop', 'database', 'logs')) {
            $script:ui.Nav[$id].PerformClick()
            [Windows.Forms.Application]::DoEvents()
            if ($script:ui.CurrentPage -ne $id) { throw "Navigation failed: $id" }
            if (-not $script:ui.SideVersions.Visible -or $sidebar.Width -ne 252) { throw 'Permanent sidebar missing' }
            foreach ($entry in $script:ui.Cards | Where-Object { $_.Page -eq $id }) {
                if (-not $entry.Button.Visible -or $entry.Button.Parent -ne $entry.Card) { throw "Lost action: $($entry.Button.Text)" }
                if ($entry.Button.Width -lt 200 -or $entry.Hint.Height -lt 30) { throw "Card is clipped: $($entry.Button.Text)" }
            }
            $bitmap = [Drawing.Bitmap]::new($script:form.Width, $script:form.Height)
            try {
                $script:form.DrawToBitmap($bitmap, [Drawing.Rectangle]::new(0, 0, $script:form.Width, $script:form.Height))
                $bitmap.Save((Join-Path $OutputDirectory "$($script:Lang)-$id-$($dimensions[0]).png"), [Drawing.Imaging.ImageFormat]::Png)
            } finally { $bitmap.Dispose() }
            $results += "$($script:Lang)/$id/$($dimensions[0]): OK"
        }
    }
    if ($dockerButton.Parent -or $dockerButton.Visible) { throw 'Docker button still displayed' }
    if ($playButton.Text -match '^\s*2\.') { throw 'Play still numbered' }
    $script:ui.LogToggle.PerformClick()
    if ($script:logBox.Visible) { throw 'Hide log failed' }
    $script:ui.LogToggle.PerformClick()
    if (-not $script:logBox.Visible) { throw 'Show log failed' }
    [Windows.Forms.Application]::DoEvents()
    if ($script:logBox.SelectionStart -ne $script:logBox.TextLength) { throw 'Log caret did not follow expansion' }
    if (([Uri](Get-UIRatesUrl)).AbsolutePath -ne '/rates') { throw 'Incorrect rates route' }
    if (-not $coffeeButton.Visible) { throw 'Coffee link is not visible' }
    $expected = @($installButton, $playButton, $stopButton, $panelButton, $clientButton, $updateButton,
        $bundleButton, $diagnosticsButton, $openLogButton, $folderButton, $botCountButton, $importDbButton,
        $repairDbButton, $dbAccessButton, $gmPanelButton, $worldBackupButton, $difficultyButton, $languageButton, $ratesButton)
    if ($coopButton) { $expected += $coopButton }
    foreach ($button in $expected) {
        if (@($script:ui.Cards | Where-Object { $_.Button -eq $button }).Count -ne 1) { throw "Missing/duplicate action: $($button.Text)" }
    }
    [pscustomobject]@{ Checks = $results; ActionCards = $expected.Count; Navigation = 'OK'; Sidebar = 'OK'; LogToggle = 'OK'; LogScroll = 'OK'; RatesRoute = 'OK'; CoffeeLink = 'OK'; ResizeMsPerStep = $resizeMsPerStep; ResizeReleaseMs = $releaseMs } | ConvertTo-Json -Depth 4
    $script:form.Close()
}
