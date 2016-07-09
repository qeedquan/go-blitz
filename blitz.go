package main

import (
	"flag"
	"image"
	"log"
	"math"
	"math/rand"
	"os"
	"path/filepath"
	"runtime"
	"time"

	"github.com/qeedquan/go-media/image/imageutil"
	"github.com/qeedquan/go-media/sdl"
	"github.com/qeedquan/go-media/sdl/sdlimage"
	"github.com/qeedquan/go-media/sdl/sdlimage/sdlcolor"
	"github.com/qeedquan/go-media/sdl/sdlmixer"
)

type Display struct {
	*sdl.Window
	*sdl.Renderer
}

type Entity struct {
	x, y    int
	between int
	next    int
}

type SubImager interface {
	SubImage(image.Rectangle) image.Image
}

type State int

const (
	INVALID State = iota
	QUIT
	START
	PREPLAY
	PLAY
	PAUSE
	STRIKE
	LOSE
	WIN
)

const (
	WIDTH          = 640
	HEIGHT         = 480
	DELAY          = 10
	STARTLEVEL     = 1
	STORYWIDTH     = 20
	STORYHEIGHT    = 20
	SCOREINCREMENT = 20
	PLANEINC       = 1
	BOMBINC        = 10
)

var (
	assets     = flag.String("assets", "assets", "data directory")
	fullscreen = flag.Bool("fullscreen", false, "fullscreen")
	mute       = flag.Bool("mute", false, "mute")

	screen     *Display
	sprites    *sdl.Texture
	building   *sdl.Texture
	explosion  *sdlmixer.Chunk
	state      State
	prevState  State
	level      int
	keyPressed bool
	total      int
	score      int
	bcount     int
	yrise      int
	yshake     int
	countdown  = 10
	bheight    [32]int
	plane      Entity
	bomb       Entity
)

func main() {
	runtime.LockOSThread()
	rand.Seed(time.Now().UnixNano())
	flag.Parse()
	initSDL()
	load()
	play()
}

func ck(err error) {
	if err != nil {
		sdl.LogCritical(sdl.LOG_CATEGORY_APPLICATION, "%v", err)
		if screen != nil {
			sdl.ShowSimpleMessageBox(sdl.MESSAGEBOX_ERROR, "Error", err.Error(), screen.Window)
		}
		os.Exit(1)
	}
}

func ek(err error) bool {
	if err != nil {
		sdl.LogError(sdl.LOG_CATEGORY_APPLICATION, "%v", err)
		return true
	}
	return false
}

func newDisplay(w, h int, flag sdl.WindowFlags) *Display {
	window, renderer, err := sdl.CreateWindowAndRenderer(w, h, flag)
	ck(err)
	return &Display{window, renderer}
}

func initSDL() {
	err := sdl.Init(sdl.INIT_EVERYTHING &^ sdl.INIT_AUDIO)
	ck(err)

	err = sdl.Init(sdl.INIT_AUDIO)
	ek(err)

	err = sdlmixer.OpenAudio(44100, sdl.AUDIO_S16, 2, 8192)
	ek(err)

	wflag := sdl.WINDOW_RESIZABLE
	if *fullscreen {
		wflag |= sdl.WINDOW_FULLSCREEN_DESKTOP
	}
	screen = newDisplay(WIDTH, HEIGHT, wflag)

	screen.SetTitle("Blitz")
	screen.SetLogicalSize(WIDTH, HEIGHT)

	sdl.ShowCursor(0)
}

func load() {
	log.SetPrefix("load: ")

	m, err := imageutil.LoadFile(filepath.Join(*assets, "spritesheet.bmp"))
	ck(err)

	img := imageutil.ColorKey(m, sdl.Color{20, 52, 87, 255})

	sprites, err = sdlimage.LoadTextureImage(screen.Renderer, img)
	ck(err)

	icon := img.(SubImager).SubImage(image.Rect(600, 0, 632, 32))
	surface, err := sdlimage.LoadSurfaceImage(icon)
	ck(err)
	screen.SetIcon(surface)
	surface.Free()

	explosion, err = sdlmixer.LoadWAV(filepath.Join(*assets, "explosion.wav"))
	ek(err)

	building, err = screen.CreateTexture(sdl.PIXELFORMAT_ARGB8888, sdl.TEXTUREACCESS_TARGET, WIDTH, HEIGHT)
	ck(err)
	building.SetBlendMode(sdl.BLENDMODE_BLEND)
}

func play() {
	prevState = INVALID
	state = START
	nextTime := uint32(0)
	for state != QUIT {
		screen.SetDrawColor(sdlcolor.Black)
		screen.Clear()
		screen.Copy(sprites, &sdl.Rect{0, HEIGHT, WIDTH, HEIGHT}, &sdl.Rect{0, 0, WIDTH, HEIGHT})
		action()
		event()
		screen.Present()
		if now := sdl.GetTicks(); now < nextTime {
			sdl.Delay(nextTime - now)
		}
		nextTime = sdl.GetTicks() + DELAY
	}
}

func action() {
	switch state {
	case START:
		startGame()
	case PREPLAY:
		preplayGame()
	case PLAY:
		playGame()
	case PAUSE:
		pauseGame()
	case STRIKE:
		strikeGame()
	case LOSE:
		loseGame()
	case WIN:
		winGame()
	}
}

func event() {
	for {
		ev := sdl.PollEvent()
		if ev == nil {
			break
		}
		switch ev := ev.(type) {
		case sdl.QuitEvent:
			state = QUIT

		case sdl.KeyDownEvent:
			keyDown(ev.Sym)
		}
	}
}

func keyDown(key sdl.Keycode) {
	switch {
	case key == sdl.K_LALT || key == sdl.K_RALT:
	case key == sdl.K_ESCAPE:
		escapeKey()
	case key == sdl.K_s:
		*mute = !*mute
	case key == sdl.K_p:
		pause()
	case state == LOSE:
		prevState = state
		state = START
	case state == START:
		prevState = state
		state = PREPLAY
	case state == WIN:
		prevState = state
		level++
		state = PREPLAY
	default:
		keyPressed = true
	}
}

func escapeKey() {
	if state == START {
		state = QUIT
	} else {
		state = START
	}
}

func pause() {
	if state != PAUSE {
		if state == PLAY || state == STRIKE {
			prevState = state
			state = PAUSE
		}
	} else {
		state = prevState
	}
}

func reset() {
	keyPressed = false
	level = STARTLEVEL
	prevState = START
	score = 0
	total = -1
	resetEntities()
}

func resetEntities() {
	plane = Entity{x: 0, y: 10, between: 2, next: 1}
	bomb = Entity{x: 0, y: -30}
}

func startGame() {
	if prevState != START {
		reset()
	}
	showInstructions()
}

func showInstructions() {
	screen.Copy(sprites, &sdl.Rect{237, 327, 353, 95}, &sdl.Rect{143, 0, 353, 95})
	screen.Copy(sprites, &sdl.Rect{0, 327, 236, 87}, &sdl.Rect{202, 197, 236, 87})
	screen.Copy(sprites, &sdl.Rect{244, 422, 388, 55}, &sdl.Rect{130, 420, 388, 55})
}

func preplayGame() {
	resetEntities()
	drawBuildings()
	bcount = len(bheight)
	keyPressed = false
	yshake = 0
	prevState = PREPLAY
	state = PLAY
}

func drawBuildings() {
	tallest := HEIGHT

	err := screen.SetTarget(building)
	if err != nil {
		log.Fatal(err)
	}
	defer screen.SetTarget(nil)

	screen.SetDrawColor(sdlcolor.Transparent)
	screen.Clear()

	for i := range bheight {
		c := rand.Intn(4)
		x := 328 + STORYWIDTH*c
		x1 := STORYWIDTH * i
		height := 1 + (rand.Intn(3) - rand.Intn(6)) + 2*level + 6
		y1 := 0
		j := 0
		for ; j < height; j++ {
			y1 = HEIGHT - STORYHEIGHT - j*STORYHEIGHT
			screen.Copy(sprites, &sdl.Rect{int32(x), 99, STORYWIDTH, STORYHEIGHT}, &sdl.Rect{int32(x1), int32(y1), STORYWIDTH, STORYHEIGHT})
		}
		bheight[i] = j
		if y1 < tallest {
			tallest = y1
		}
	}
	if level == 1 {
		yrise = HEIGHT - tallest
	} else {
		yrise = 0
	}
}

func playGame() {
	blitMoon()
	blitScore()
	blitBuildings()
	blitPlane()
	blitBomb()
	movePlane()
	moveBomb()
}

func pauseGame() {
	blitMoon()
	blitScore()
	blitBuildings()
	blitPlane()
	if prevState == STRIKE {
		blitExplosion()
	} else {
		blitBomb()
	}
	screen.Copy(sprites, &sdl.Rect{478, 0, 50, 50}, &sdl.Rect{295, 220, 50, 50})
}

func strikeGame() {
	blitMoon()
	blitScore()
	blitBuildings()
	blitPlane()
	blitExplosion()
	movePlane()
	moveExplosion()
}

func loseGame() {
	blitMoon()
	blitScore()
	blitBuildings()
	screen.Copy(sprites,
		&sdl.Rect{0, 32, 128, 120},
		&sdl.Rect{int32(plane.x) - 25, int32(plane.y) - 15, 128, 120},
	)
	screen.Copy(sprites,
		&sdl.Rect{0, 153, 559, 86},
		&sdl.Rect{41, 197, 559, 86},
	)
}

func winGame() {
	blitMoon()
	blitScore()
	blitBuildings()
	screen.Copy(sprites,
		&sdl.Rect{0, 239, 548, 88},
		&sdl.Rect{46, 196, 548, 88},
	)
}

func blitPlane() {
	x := 528
	if state != PAUSE {
		x = 200
	}
	screen.Copy(sprites, &sdl.Rect{int32(x), 0, 67, 40}, &sdl.Rect{int32(plane.x), int32(plane.y), 67, 40})
}

func blitMoon() {
	x := 403
	if rand.Int31() > math.MaxInt32/50 && state != PAUSE {
		x = 328
	}
	screen.Copy(sprites, &sdl.Rect{int32(x), 0, 75, 99}, &sdl.Rect{555, 10, 75, 99})
}

func blitBuildings() {
	screen.Copy(
		building,
		&sdl.Rect{0, 0, WIDTH, HEIGHT - int32(yrise)},
		&sdl.Rect{0, int32(-yshake + yrise), WIDTH, HEIGHT - int32(yrise)},
	)
	if yrise > 0 {
		yrise -= 5
	}
}

func blitScore() {
	xdigit := []int32{67, 80, 93, 106, 119, 132, 145, 158, 171, 184}
	x := 70

	if score-total > SCOREINCREMENT {
		total = score
	} else if total < score {
		total++
	}

	for i := total; i > 0; i /= 10 {
		x += 11
	}

	screen.Copy(sprites, &sdl.Rect{0, 0, 64, 32}, &sdl.Rect{10, 10, 64, 32})
	i := total
	for r := i % 10; i > 0; {
		screen.Copy(sprites,
			&sdl.Rect{xdigit[r], 0, 11, 32},
			&sdl.Rect{int32(x), 10, 11, 32},
		)
		x -= 11
		i /= 10
		r = i % 10
	}
}

func blitBomb() {
	if bomb.y > 0 {
		screen.Copy(sprites,
			&sdl.Rect{267, 0, 20, 30},
			&sdl.Rect{int32(bomb.x), int32(bomb.y), 20, 30},
		)
	}
}

func blitExplosion() {
	screen.Copy(sprites,
		&sdl.Rect{0, 32, 128, 120},
		&sdl.Rect{int32(bomb.x) - 50, int32(bomb.y) - 30, 128, 120},
	)
}

func movePlane() {
	if plane.next == 0 {
		plane.x += PLANEINC
		if plane.x > WIDTH {
			plane.x = -67
			plane.y += 10
		}
		plane.next = plane.between - 1
	} else {
		plane.next--
	}

	checkForCrash()
}

func checkForCrash() {
	col := (plane.x + 37) / STORYWIDTH
	if col >= 0 && col < 32 {
		if plane.y > HEIGHT-bheight[col]*STORYHEIGHT-40 {
			state = LOSE
		}
	}
}

func moveBomb() {
	if keyPressed && bomb.y == -30 {
		col := plane.x / STORYWIDTH
		if col >= 0 && col < 32 {
			bomb.x = col * STORYWIDTH
			bomb.y = plane.y + 30
		}
	} else if bomb.y != -30 {
		bomb.y += BOMBINC
		checkForStrike()
	}

	if bomb.y > HEIGHT {
		bomb.y = -30
		keyPressed = false
	}
}

func checkForStrike() bool {
	col := bomb.x / STORYWIDTH

	if bomb.y > HEIGHT-bheight[col]*STORYHEIGHT-30 {
		if bheight[col] != 0 {
			state = STRIKE
			removeStory(col)
			playSound()

			score += SCOREINCREMENT
			if bheight[col]--; bheight[col] == 0 {
				bcount--
			}
			if bcount == 0 {
				state = WIN
			}
		}
		return true
	}
	return false
}

func removeStory(col int) {
	err := screen.SetTarget(building)
	if err != nil {
		log.Fatal(err)
	}
	defer screen.SetTarget(nil)

	screen.SetDrawColor(sdlcolor.Transparent)
	screen.FillRect(&sdl.Rect{
		int32(col) * STORYWIDTH,
		HEIGHT - STORYHEIGHT*int32(bheight[col]),
		STORYWIDTH,
		STORYHEIGHT,
	})
}

func moveExplosion() {
	countdown--
	yshake = countdown % 3
	if countdown == 0 {
		yshake = 0
		bomb.y = -30
		state = PLAY
		countdown = 10
		keyPressed = false
	}
}

func playSound() {
	if *mute || explosion == nil {
		return
	}
	explosion.PlayChannel(-1, 0)
}
