import * as CameraApi from '../../stubs/public_api';

const CommonAspectTests = (api: typeof CameraApi) => {
    describe(
        'Aspect',
        () => {
            test(
                'Aspect for "Aspect.ID.full"',
                () => {
                    const FileFormat = new api.Aspect(api.Aspect.ID.full);
                    expect(FileFormat.value).toStrictEqual(api.Aspect.ID.full);
                    expect(FileFormat.label).toStrictEqual('full');
                },
            );
            test(
                'Aspect for "Aspect.ID.apsc"',
                () => {
                    const FileFormat = new api.Aspect(api.Aspect.ID.apsc);
                    expect(FileFormat.value).toStrictEqual(api.Aspect.ID.apsc);
                    expect(FileFormat.label).toStrictEqual('apsc');
                },
            );
        },
    );
};
export default CommonAspectTests;
