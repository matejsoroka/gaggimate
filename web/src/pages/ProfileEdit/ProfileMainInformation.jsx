import Card from '../../components/Card.jsx';

export function ProfileMainInformation(props) {
  console.log(props.data);
  return (
    <Card sm={10} title='Profile Information'>
      <div className='form-control'>
        <label htmlFor='label' className='mb-2 block text-sm font-medium'>
          Title
        </label>
        <input
          id='label'
          name='label'
          className='input input-bordered w-full'
          value={props.data?.label}
          onChange={props.onChangeLabel}
          aria-label='Enter a name for this profile'
          required
        />
      </div>
      <div className='form-control'>
        <label htmlFor='description' className='mb-2 block text-sm font-medium'>
          Description
        </label>
        <textarea
          id='description'
          name='description'
          rows={2}
          className='textarea textarea-bordered w-full text-wrap'
          value={props.data?.description}
          onChange={props.onChangeDescription}
          aria-label='Optional description for this profile'
        />
      </div>
      <div className='form-control'>
        <label htmlFor='temperature' className='mb-2 block text-sm font-medium'>
          Temperature
        </label>
        <div className='input-group'>
          <label htmlFor='temperature' className='input w-full'>
            <input
              id='temperature'
              name='temperature'
              type='number'
              className='grow'
              value={props.data?.temperature}
              onChange={props.onChangeTemperature}
              aria-label='Temperature in degrees Celsius'
              min='0'
              max='150'
              step='0.1'
            />
            <span aria-label='degrees Celsius'>°C</span>
          </label>
        </div>
      </div>
      <div className='form-control'>
        <label htmlFor='roast' className='mb-2 block text-sm font-medium'>
          Roast level
        </label>
        <select
          id='roast'
          name='roast'
          className='select select-bordered w-full'
          value={props.data?.roast || ''}
          onChange={props.onChangeRoast}
          aria-label='Roast level of the beans this profile is for'
        >
          <option value=''>Not set</option>
          <option value='light'>Light</option>
          <option value='medium'>Medium</option>
          <option value='dark'>Dark</option>
        </select>
        <div className='mt-1 text-xs opacity-70'>
          Used by the Shot Coach to judge shot time, ratio and temperature for this roast.
        </div>
      </div>
      <div className='form-control'>
        <label
          htmlFor='utility'
          className='mb-2 block text-sm font-medium'
          aria-label='Utility profile toggle'
        >
          Utility profile
        </label>
        <input
          id='utility'
          name='utility'
          type='checkbox'
          className='toggle toggle-primary'
          checked={!!props.data?.utility}
          onChange={props.onChangeUtility}
        />
      </div>
    </Card>
  );
}
