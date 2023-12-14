import { PropertyValue } from './CameraProperty';

export class Aspect implements PropertyValue {

    [Symbol.toStringTag] = 'Aspect';

    private readonly label_: string;

    /**
     * @class FileFormat
     * @param {number} value_
     */
    constructor(
        private readonly value_: number
    ) {
        this.label_ = Object
            .keys(Aspect.ID)
            .find(key => Aspect.ID[key] === this.value_) ||
                `0x${value_.toString(16).padStart(8, '0')}`;
    }

    /**
     * @readonly
     * @type {string}
     */
    get label(): string {
        return this.label_;
    }

    /**
     * @readonly
     * @type {number}
     */
    get value(): number {
        return this.value_;
    }

    /**
     * Allows type cast to number - returns the value.
     * @param {string} hint
     * @return {number|string|null}
     */
    [Symbol.toPrimitive](hint: string): string | number | null {
        return this.value_;
        switch (hint) {
            case 'number':
                return this.value_;
            case 'string':
                return this.label;
            default:
                return null;
        }
    }


    /**
     * @readonly
     * @enum {number}
     */
    static readonly ID: {[key: string]: number} = {
        'full': 0x0000,
        'square': 0x0001,
        '4:3': 0x0002,
        '16:9': 0x0007,
        'apsc': 0x000d
    };

    // GenerateEnd
}
