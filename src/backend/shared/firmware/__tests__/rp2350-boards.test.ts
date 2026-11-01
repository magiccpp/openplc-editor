import { existsSync, readFileSync } from 'node:fs'
import { join } from 'node:path'

import { BoardInfoResolver, type HalsFileContent, type PackageManagerPort } from '../../hardware/board-info-resolver'
import { getHalsFile } from '../hals-loader'

const HAL_DIR = join(__dirname, '..', '..', '..', '..', '..', 'resources', 'sources', 'hal')

const noPackages: PackageManagerPort = {
  listInstalled: () => [],
  getInstalledPackageManifest: () => null,
}

function resolver(): BoardInfoResolver {
  return new BoardInfoResolver({
    halsContent: getHalsFile<HalsFileContent>(),
    packageManager: noPackages,
    resolveHalSourcePath: (rel) => join(HAL_DIR, rel),
    resolvePackageRelativePath: (pkg, rel) => join(pkg, rel),
  })
}

describe('built-in RP2350 boards', () => {
  it.each([
    ['Raspberry Pico 2 (RP2350)', 'rp2040:rp2040:rpipico2', undefined],
    ['Raspberry Pico 2 W (RP2350)', 'rp2040:rp2040:rpipico2w', 'BOARD_PICOW'],
  ])('%s resolves to the arduino-pico core with the RP2350 HAL', (name, platform, define) => {
    const info = resolver().resolve(name)
    expect(info.source).toBe('hals')
    expect(info.compiler).toBe('arduino-cli')
    expect(info.core).toBe('rp2040:rp2040')
    expect(info.platform).toBe(platform)
    expect(info.define).toBe(define)
    expect(info.halSourceFile).toBe(join(HAL_DIR, 'rp2350pico.cpp'))
    expect(info.coreVersion).toBe('6.3.2')
    expect(info.debug?.channels.map((c) => c.channel)).toEqual(['rtu'])
  })

  it('maps a hals.json core_version onto coreVersion and leaves it unset otherwise', () => {
    const r = new BoardInfoResolver({
      halsContent: {
        Pinned: { compiler: 'arduino-cli', core: 'rp2040:rp2040', core_version: '6.3.2' },
        Unpinned: { compiler: 'arduino-cli', core: 'rp2040:rp2040' },
      },
      packageManager: noPackages,
      resolveHalSourcePath: (rel) => join(HAL_DIR, rel),
      resolvePackageRelativePath: (pkg, rel) => join(pkg, rel),
    })
    expect(r.resolve('Pinned').coreVersion).toBe('6.3.2')
    expect(r.resolve('Unpinned').coreVersion).toBeUndefined()
  })

  it('does not reuse a VPP device name, so hals.json cannot shadow the package boards', () => {
    const names = Object.keys(getHalsFile<HalsFileContent>())
    expect(names).not.toContain('Raspberry Pico 2')
    expect(names).not.toContain('Raspberry Pico 2 W')
  })

  describe('rp2350pico.cpp', () => {
    const src = readFileSync(join(HAL_DIR, 'rp2350pico.cpp'), 'utf8')

    it('exists next to the other HAL sources', () => {
      expect(existsSync(join(HAL_DIR, 'rp2350pico.cpp'))).toBe(true)
    })

    it('enables the internal pull-up on digital inputs (RP2350-E9)', () => {
      expect(src).toMatch(/pinMode\(pinMask_DIN\[i\], INPUT_PULLUP\)/)
    })

    it('never drives GP25, the CYW43 chip select on the Pico 2 W', () => {
      expect(src).not.toMatch(/pinMode\(\s*25\s*,/)
    })

    it('does not depend on the RP2040-only PWM library', () => {
      expect(src).not.toMatch(/#include\s+"RP2040_PWM\.h"/)
    })
  })
})
